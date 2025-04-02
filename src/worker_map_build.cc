#include <cassert>
#include <ranges>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <print>
#include <future>
#include <optional>
#include <tuple>
#include "raylib.h"
#include "map_data.hpp"
#include "earcut.hpp"
#include "worker_map_build.hpp"

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
  m.job_params = std::make_optional(std::move(params));
  m.job_cond.notify_one();
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
  size_t num_chunks = m.job_params->chunks.size();
  vector<ExpectedJobResult> job_results;
  job_results.reserve(num_chunks);
  // it has to be initialized i guess
  for (size_t i = 0; i < num_chunks; ++i) 
    job_results.push_back(JobResult {});

  vector<HttpResponse> responses = fetch_map_data(m.job_params->chunks);

  // read responses for potential errors
  for (size_t i = 0; i < responses.size(); ++i) {
    const HttpResponse& r = responses[i];
    // not very explicit, means curl failed, not the requests themselves
    // could also use some error code
    if (r.status_code == -1) {
      TraceLog(LOG_ERROR, "WORKER: [MAP BUILD] Job failed, fatal error occured");
      m.job_params->promise.set_value(vector<ExpectedJobResult> { unexpected(ErrorInternal{}) });
      return;
    }

    if (r.status_code >= 400) {
      job_results[i] = unexpected(ErrorHttp {r.status_code, r.data}); 
    }
  }

  const auto xml_resps = responses | views::transform([](const HttpResponse& r) -> optional<string_view> { 
    if (r.status_code >= 400) {
      return nullopt;
    } else {
      return string_view(r.data);
    }
  });
  vector<MapData> mds = parse_map_data(xml_resps);

  for (size_t i = 0; i < mds.size(); ++i) {
    if (!job_results[i].has_value()) continue; // maybe already holds an http error

    // because it might not be immediatly clear,
    // this expression selects buildings, earcuts each one into a list of triangles
    // then meshes are generated from these triangles
    MapData& md = mds[i];
    job_results[i]->meshes = [&md]() {
      auto buildings = md.ways | views::filter([](const Way& w){ return w.is_building(); });
      vector<EarcutResult> earcuts = earcut_collection(std::move(buildings));
      return build_meshes(earcuts);
    }();

    auto roads_view = md.ways | views::filter([](const Way& w) { return w.is_highway(); });
    job_results[i]->roads = vector<Way>(roads_view.begin(), roads_view.end());
  }

  m.job_params->promise.set_value(job_results);
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Job finished");
}
