#include "map_build_job.hpp"
#include <vector>
#include <queue>
#include <memory>
#include <cassert>
#include <format>
#include <string>
#include <string_view>
#include <expected>
#include <optional>
#include "curl/curl.h"
#include "map_data.hpp"
#include "earcut.hpp"

using namespace std;
using ExpectedJobResult = MapBuildJob::ExpectedJobResult;
using JobResult = MapBuildJob::JobResult;
using JobError = MapBuildJob::JobError;

MapBuildJob::MapBuildJob(): 
  m {}
{
  m.curlm = curl_multi_init();
}

MapBuildJob::~MapBuildJob() {
  curl_multi_cleanup(m.curlm);
}

static size_t curl_wrcb(char* ptr, size_t size, size_t nmemb, void* ud) {
  (void)size;
  *((string*)ud) += string(ptr, nmemb);
  return nmemb;
}

void MapBuildJob::start(const vector<shared_ptr<Chunk>>& chunks) {
  if (chunks.size() == 0) {
    m.state = State::Finished;
    return;
  }

  m.state = State::Working;
  m.ongoing.clear();
  m.ongoing.reserve(chunks.size());

  for (auto& chunk : chunks) {
    double longA = chunk->min_lon;
    double latA = chunk->min_lat;
    double longB = chunk->max_lon;
    double latB = chunk->max_lat;
    chunk->status = ChunkStatus::Generating;

    m.ongoing.push_back(OngoingJob {});
    OngoingJob& job = m.ongoing.back();
    job.target = chunk;
    job.curl = curl_easy_init();
    curl_easy_setopt(job.curl, CURLOPT_URL, format("https://www.openstreetmap.org/api/0.6/map?bbox={},{},{},{}", longA, latA, longB, latB).c_str());
    curl_easy_setopt(job.curl, CURLOPT_WRITEFUNCTION, curl_wrcb);
    curl_easy_setopt(job.curl, CURLOPT_WRITEDATA, &job.data);
    curl_multi_add_handle(m.curlm, job.curl);
  }
}

expected<JobResult, JobError> MapBuildJob::try_build_job_result(OngoingJob& ongoing_job) {
  string_view xml_resp = string_view(ongoing_job.data);
  optional<MapData> md = parse_map_data(xml_resp);
  if (!md) {
    return unexpected(ErrorInternal {});
  }

  return JobResult {
    .roads = [&md]() {
      auto roads_view = md->ways | views::filter([](const Way& w){ return w.is_highway(); });
      return vector<Way>(roads_view.begin(), roads_view.end());
    }(),
    .meshes = [&md](){ 
      auto buildings = md->ways | views::filter([](const Way& w){ return w.is_building(); });
      vector<EarcutResult> earcuts = earcut_collection(std::move(buildings));
      return build_meshes(earcuts);
    }()
  };
}

queue<ExpectedJobResult> MapBuildJob::poll() {
  if (m.state == State::Finished || m.state == State::AwaitingStart) return {};
  curl_multi_poll(m.curlm, NULL, 0, 0, NULL);

  int running_handles;
  curl_multi_perform(m.curlm, &running_handles);

  // whilst they are not running, they might need to be processed
  // let's not be hasty into returning earlier than needed
  if (running_handles == 0) {
    m.state = State::Finished;
  }

  queue<ExpectedJobResult> results {};

  CURLMsg* msg;
  while (msg = curl_multi_info_read(m.curlm, &running_handles), msg) {
    switch(msg->msg) {
      case CURLMSG_DONE: {
          auto ongoing_job = ranges::find_if(m.ongoing.begin(), m.ongoing.end(),
            [&](const auto& job){ 
              return job.curl == msg->easy_handle; 
            }
          );
          assert(ongoing_job != m.ongoing.end() && "No ongoing job found for a request. FATAL ERROR");

          long code;
          curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &code);
          if (code >= 400) {
            results.push(ExpectedJobResult {
              ongoing_job->target,
              unexpected(ErrorHttp {
                code,
                ongoing_job->data,
              })
            });
          } else {
            results.push({
              ongoing_job->target,
              try_build_job_result(*ongoing_job)
            });  
          }
          
          m.ongoing.erase(ongoing_job);
          curl_multi_remove_handle(m.curlm, ongoing_job->curl);
          curl_easy_cleanup(ongoing_job->curl);
        }
        break;
      default:
        break;
    } 
  } 
  return results;
}
