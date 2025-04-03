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
  end();
}

void WorkerMapBuild::start_idling() {
  assert(!m.thr.joinable() && "Called start_idling() twice on WorkerMapBuild");
  m.thr = thread(&WorkerMapBuild::idle_job, this);
}

void WorkerMapBuild::idle_job() {
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Started, waiting for job start");
  while (true) {
    unique_lock lock(m.mutex);   
    m.job_cond.wait(lock, [this]() { return m.job_params.has_value() || m.should_stop; });
    if (m.should_stop) return;
    job();
    m.job_params = std::nullopt;
  }
}

void WorkerMapBuild::start_job(JobParams&& params) {
  if (m.job_params.has_value()) {
    TraceLog(LOG_WARNING, "WORKER [MAP BUILD] Job ignored, there is already one running");
    return;
  }
  m.job_params = std::make_optional(std::move(params));
  m.results_queue = {};
  m.job_cond.notify_one();
}

queue<WorkerMapBuild::ExpectedJobResult> WorkerMapBuild::take_results() {
  // this seems to be sub optimized, there has got to be a better way
  auto moved = std::move(m.results_queue);
  m.results_queue = {};
  return moved;
}

void WorkerMapBuild::end() {
  m.should_stop = true;
  m.job_cond.notify_one();
  if (m.thr.joinable()) {
    m.thr.join();
  }
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Exited successfully");
}

void WorkerMapBuild::job() {
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Job started");
  for (auto& chunk : m.job_params->chunks) 
    chunk->status(ChunkStatus::Generating);

  // this has to be done as a first step, ideally we'd have a "network worker"
  // doing that and sending the results as they come so we can start parsing
  // without having to wait for every results
  vector<HttpResponse> responses = fetch_map_data(m.job_params->chunks);

  // read responses for potential errors
  for (HttpResponse& r : responses) {
    // not very explicit, means curl failed, not the requests themselves
    // could also use some error code
    if (r.status_code == -1) {
      TraceLog(LOG_ERROR, "WORKER: [MAP BUILD] Job failed, fatal error occured");
      m.results_queue.push({ r.target, unexpected(ErrorInternal{}) });
      for (auto& chunk : m.job_params->chunks) 
        chunk->status(ChunkStatus::Invalid);
      // we can end the job early
      end();
      return;
    }

    if (r.status_code >= 400) {
      r.target->status(ChunkStatus::Invalid);
      m.results_queue.push({ r.target, unexpected(ErrorHttp {r.status_code, r.data}) }); 
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

    m.results_queue.push({r.target, std::move(res)});
  }

  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Job finished");
}
