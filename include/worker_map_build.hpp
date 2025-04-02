#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <future>
#include <optional>
#include <variant>
#include <expected>
#include "chunk.hpp"
#include "map_data.hpp"
#include "earcut.hpp"

class WorkerMapBuild {
public:
  struct JobResult {
    // if i feel like it, i'll make a proper "Road" type someday instead of using the raw
    // parsed data
    std::vector<Way> roads;
    std::vector<EarcutMesh> meshes;
  };

  struct ErrorHttp {
    long code;
    std::string msg;
  };

  struct ErrorInternal {};

  using JobError = std::variant<ErrorHttp, ErrorInternal>;

  using ExpectedJobResult = std::expected<JobResult, JobError>;
  struct JobParams {
    std::promise<std::vector<ExpectedJobResult>> promise;
    std::vector<Chunk> chunks;
  };

public:
  WorkerMapBuild() = default;
  ~WorkerMapBuild();
  void start_idling();
  void start_job(JobParams&& params);
  void end();
private:
  struct M {
    std::optional<JobParams> job_params = std::nullopt;

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
