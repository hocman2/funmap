#include <ranges>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <print>
#include <future>
#include "raylib.h"
#include "map_data.hpp"
#include "earcut.hpp"
#include "worker_map_build.hpp"

using namespace std;
namespace views = ranges::views;

WorkerMapBuild::WorkerMapBuild():
  t_start_job(false), t_close(false) {}

void WorkerMapBuild::run_idling() {
  t_thr = thread(&WorkerMapBuild::idle_job, this);
}

void WorkerMapBuild::idle_job() {
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Started, waiting for job start");
  while (true) {
    unique_lock lock(t_mutex);   
    t_job_cond.wait(lock, [this]() { return t_start_job || t_close; });
    if (t_close) return;
    job();
    t_start_job = false;
  }
}

void WorkerMapBuild::start_job(WorkerMapBuildJobParams&& params) {
  t_start_job = true;
  t_params = std::move(params);
  t_job_cond.notify_one();
}

void WorkerMapBuild::end() {
  t_close = true;
  t_job_cond.notify_one();
  if (t_thr.joinable()) {
    t_thr.join();
  }
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Exited successfully");
}

void WorkerMapBuild::job() {
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Job started");
  MapData md;
  fetch_and_parse(&md, t_params.longA, t_params.latA, t_params.longB, t_params.latB);

  // because it might not be immediatly clear,
  // this expression selects buildings, earcuts each one into a list of triangles
  // then meshes are generated from these triangles
  WorkerMapBuildJobResult res;
  res.meshes = [&md]() {
    auto buildings = md.ways | views::filter([](const Way& w){ return w.is_building(); });
    vector<EarcutResult> earcuts = earcut_collection(std::move(buildings));
    return build_meshes(earcuts);
  }();

  auto roads_view = md.ways | views::filter([](const Way& w) { return w.is_highway(); });
  res.roads = vector<Way>(roads_view.begin(), roads_view.end());

  // fulfill the promise
  t_params.promise.set_value(res);
  TraceLog(LOG_INFO, "WORKER: [MAP BUILD] Job finished");
}
