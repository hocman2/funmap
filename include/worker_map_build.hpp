#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <future>
#include <optional>
#include "map_data.hpp"
#include "earcut.hpp"

struct WorkerMapBuildJobResult {
  // if i feel like it, i'll make a proper "Road" type someday instead of using the raw
  // parsed data
  std::vector<Way> roads;
  std::vector<EarcutMesh> meshes;
};

struct WorkerMapBuildJobParams {
  std::promise<WorkerMapBuildJobResult> promise;
  double longA;
  double latA;
  double longB;
  double latB;
};

class WorkerMapBuild {
public:
  WorkerMapBuild() = default;
  ~WorkerMapBuild();
  void start_idling();
  void start_job(WorkerMapBuildJobParams&& params);
  void end();
private:
  struct M {
    std::optional<WorkerMapBuildJobParams> job_params = std::nullopt;

    // job scheduling flags
    bool should_stop = false;

    // Parallel computing stuff
    std::thread thr = {};
    std::mutex mutex = {};
    std::condition_variable job_cond = {};
  } m;
private:
  void idle_job();
  void job();
};
