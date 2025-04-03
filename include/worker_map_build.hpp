#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <optional>
#include <variant>
#include <expected>
#include <memory>
#include "chunk.hpp"
#include "map_data.hpp"
#include "earcut.hpp"

class WorkerMapBuild {
public:
  struct JobResult {
    // if i feel like it, i'll make a proper "Road" type someday instead of using the raw
    // parsed data
    std::vector<std::unique_ptr<Way>> roads;
    std::vector<std::unique_ptr<EarcutMesh>> meshes;
  };

  struct ErrorHttp {
    long code;
    std::string msg;
  };

  struct ErrorInternal {};

  using JobError = std::variant<ErrorHttp, ErrorInternal>;

  struct ExpectedJobResult {
    std::shared_ptr<Chunk> target;
    std::expected<JobResult, JobError> result;
  };

  struct JobParams {
    std::vector<std::shared_ptr<Chunk>> chunks;
  };

public:
  WorkerMapBuild() = default;
  ~WorkerMapBuild();
  void start_idling();
  void start_job(JobParams&& params);
  bool has_job() const { return m.job_params.has_value(); }
  bool has_results() const { return !m.results_queue.empty(); }
  std::queue<ExpectedJobResult> take_results();
  void end();
private:
  struct M {
    std::queue<ExpectedJobResult> results_queue;
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
