#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <future>
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
  WorkerMapBuild();
  WorkerMapBuild& operator=(const WorkerMapBuild&) = delete;
  WorkerMapBuild& operator=(WorkerMapBuild&&) = delete;
  WorkerMapBuild(const WorkerMapBuild&) = delete;
  WorkerMapBuild(WorkerMapBuild&&) = delete;

  void run_idling(); 
  void start_job(WorkerMapBuildJobParams&& params);
  void end();
private:
  WorkerMapBuildJobParams t_params;

  // job scheduling
  bool t_start_job;
  bool t_close;
  std::thread t_thr;
  std::mutex t_mutex;
  std::condition_variable t_job_cond;

private:
  void idle_job();
  void job();
};
