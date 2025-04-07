#include "worker_map_build.hpp"

#include <cassert>
#include <ranges>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <print>
#include <queue>
#include <optional>
#include <tuple>
#include "raylib.h"
#include "map_data.hpp"
#include "earcut.hpp"

using namespace std;
namespace views = ranges::views;

WorkerMapBuild::~WorkerMapBuild() {
  if (m.thr.joinable()) {
    end();
    m.thr.join();
  }
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Exitted successfully");
}

void WorkerMapBuild::start_idling() {
  assert(!m.thr.joinable() && "Called start_idling() twice on WorkerMapBuild");
  m.thr = thread(&WorkerMapBuild::idle_job, this);
}

void WorkerMapBuild::idle_job() {
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Started, waiting for job start");
  while (true) {
    unique_lock lock(m_mtx);   
    m.job_cond.wait(lock, [this]() { return m.job_params.has_value() || m.should_stop; });
    if (m.should_stop) return;

    // leave data access open for the job duration
    // The job will lock when it needs but getters
    // must remain accessible
    lock.unlock();
      job();
    lock.lock();

    m.job_params = std::nullopt;
  }
}

void WorkerMapBuild::start_job(JobParams&& params) {
  lock_guard lock(m_mtx);
  if (m.job_params.has_value()) {
    TraceLog(LOG_WARNING, "WORKER [MAP BUILD] Job ignored, there is already one running");
    return;
  }
  m.job_params = std::make_optional(std::move(params));
  m.results_queue = {};
  m.job_cond.notify_one();
}

queue<WorkerMapBuild::ExpectedJobResult> WorkerMapBuild::take_results() {
  lock_guard lock(m_mtx);
  // this seems to be sub optimized, there has got to be a better way
  auto moved = std::move(m.results_queue);
  m.results_queue = {};
  return moved;
}

void WorkerMapBuild::end() {
  {
    lock_guard lock(m_mtx);
    m.should_stop = true;
  }

  m.job_cond.notify_one();
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] End notification sent");
}

void WorkerMapBuild::job() {
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Job started");

  vector<shared_ptr<Chunk>> chunks;
  {
    lock_guard l(m_mtx); 
    chunks = m.job_params->chunks;
  }

  for (auto& chunk : chunks) 
    chunk->status(ChunkStatus::Generating);

  // this has to be done as a first step, ideally we'd have a "network worker"
  // doing that and sending the results as they come so we can start parsing
  // without having to wait for every results
  vector<HttpResponse> responses = fetch_map_data(chunks);

  // read responses for potential errors
  for (HttpResponse& r : responses) {
    // not very explicit, means curl failed, not the requests themselves
    // could also use some error code
    if (r.status_code == -1) {
      TraceLog(LOG_ERROR, "WORKER: [MAP BUILD] Job failed, fatal error occured");
      {
        lock_guard l(m_mtx);
        m.results_queue.push({ r.target, unexpected(ErrorInternal{}) });
      }

      for (auto& chunk : chunks) 
        chunk->status(ChunkStatus::Invalid);
      // we can end the job early
      end();
      return;
    }

    if (r.status_code >= 400) {
      r.target->status(ChunkStatus::Invalid);
      {
        lock_guard l(m_mtx);
        m.results_queue.push({ r.target, unexpected(ErrorHttp {r.status_code, r.data}) }); 
      }
      continue;
    }

    const auto xml_resp = string_view(r.data);
    MapData md = parse_map_data(xml_resp);
    JobResult res;
    res.meshes = ([&md](){ 
      auto buildings = md.ways | views::filter([](const Way& w){ return w.is_building(); });
      vector<EarcutResult> earcuts = earcut_collection(std::move(buildings));
      return build_meshes(earcuts);
    })();

    auto roads_view = md.ways 
                | views::filter([](const Way& w){ return w.is_highway(); })
                | views::transform([](const Way& w){ return make_unique<Way>(w); });

    res.roads = vector<unique_ptr<Way>>(roads_view.begin(), roads_view.end());

    {
      lock_guard l(m_mtx);
      m.results_queue.push({r.target, std::move(res)});
    }
  }

  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Job finished");
}
