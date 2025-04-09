#pragma once
#include <vector>
#include <queue>
#include "curl/curl.h"
#include "chunk.hpp"
#include <memory>
#include <expected>
#include <variant>
#include <string>

class MapBuildJob {
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
  struct ExpectedJobResult {
    std::shared_ptr<Chunk> target;
    std::expected<JobResult, JobError> result;
  };

  struct OngoingJob {
    std::shared_ptr<Chunk> target;
    CURL* curl;
    std::string data;
  };
public:
  MapBuildJob();
  ~MapBuildJob();

  void start(const std::vector<std::shared_ptr<Chunk>>& chunks);
  std::queue<ExpectedJobResult> poll();
  bool finished() const { return m.state == State::Finished; };
private:
  std::expected<JobResult,JobError> try_build_job_result(OngoingJob& ongoing_job);
private:
  enum class State {AwaitingStart, Working, Finished};
  struct M {
    std::vector<OngoingJob> ongoing = {};
    CURLM* curlm = nullptr;
    State state = State::AwaitingStart;
  } m;
};
