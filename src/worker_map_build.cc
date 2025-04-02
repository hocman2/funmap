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
  MapData md;
  auto err = fetch_and_parse(&md, m.job_params->longA, m.job_params->latA, m.job_params->longB, m.job_params->latB);

  if (err) {
    const auto& [code, msg] = *err;
    TraceLog(LOG_ERROR, "WORKER: [MAP BUILD] Job failed: [HTTP %ld]: %s", code, msg.c_str());
    m.job_params->promise.set_value(unexpected(ErrorHttp {code, msg}));
    return;
  }

  // because it might not be immediatly clear,
  // this expression selects buildings, earcuts each one into a list of triangles
  // then meshes are generated from these triangles
  JobResult res;
  res.meshes = [&md]() {
    auto buildings = md.ways | views::filter([](const Way& w){ return w.is_building(); });
    vector<EarcutResult> earcuts = earcut_collection(std::move(buildings));
    return build_meshes(earcuts);
  }();

  auto roads_view = md.ways | views::filter([](const Way& w) { return w.is_highway(); });
  res.roads = vector<Way>(roads_view.begin(), roads_view.end());

  m.job_params->promise.set_value(res);
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Job finished");
}
