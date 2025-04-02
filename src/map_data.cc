#include "raylib.h"
#include <curl/curl.h>
#include <string>
#include <print>
#include <format>
#include <cstdint>
#include <tuple>
#include <cmath>
#include <optional>
#include "map_data.hpp"

using namespace std;

unordered_set<string> MapData::tag_keys;
unordered_set<string> MapData::tag_values;

Tag Tag::make_valueless(const char* key) noexcept { 
  // this shit will break if tag_keys isn't found !! idc this whole function is trash anyway !
  return {
    .key = std::string_view(*(MapData::tag_keys.find(key))), 
    .value = std::string_view("")
  }; 
}

bool Way::is_building() const noexcept {
  Tag valueless_building = Tag::make_valueless("building");
  auto building_tag = this->tags.find(valueless_building);
  if (building_tag == this->tags.end())
    return false;
  else if (strcmp(building_tag->value.data(), "yes") == 0)
    return true;
  else
    return false;
}

bool Way::is_highway() const noexcept {
  Tag valueless_highway = Tag::make_valueless("highway");
  auto highway_tag = this->tags.find(valueless_highway);
  if (highway_tag == this->tags.end())
    return false;
  else
    return true;
}

static double ref_lon = 0.0; static double ref_lat = 0.0;
static const double EARTH_RAD = 6371.0 * 100.0; // <- this scale factor should be ajusted for convenience 100 -> 1u=1dm, 1000 -> 1u=1m
void setProjectionReference(double lon, double lat) {
  ref_lon = lon;
  ref_lat = lat;
}

Vector2 to2DCoords(double lon, double lat) {
  double dlat = (lat - ref_lat) * M_PI / 180.0;
  double dlon = (lon - ref_lon) * M_PI / 180.0;
  return Vector2 {
    .x = (float)(EARTH_RAD * dlon * cos(ref_lat * M_PI / 180.0)),
    .y = -(float)(EARTH_RAD * dlat),
  };
}

pair<double, double> toMapCoords(Vector2 v) {
  const static float PI_EARTH = M_PI * EARTH_RAD;
  return make_pair(
    (((180.0f * v.x) / (PI_EARTH * cos(ref_lat * M_PI / 180.0))) + ref_lon),
    // Inverted on y axis because we are converting to an XZ plane
    // where Z goes in the opposite direction of OGL's Z
    (((180.0f * -v.y) / PI_EARTH) + ref_lat)
  );
}

static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  (void)size;

  *((string*)userdata) += string(ptr, nmemb); 
  return nmemb;
}

vector<HttpResponse> fetch_map_data(const vector<Chunk>& chunks) {
  vector<CURL*> easy_handles;
  easy_handles.reserve(chunks.size());
  vector<HttpResponse> responses;
  responses.reserve(chunks.size());

  CURLM* curlm = curl_multi_init();

  for (size_t i = 0; i < chunks.size(); ++i) {
    double longA = chunks[i].min_lon; 
    double latA = chunks[i].min_lat; 
    double longB = chunks[i].max_lon; 
    double latB = chunks[i].max_lat; 

    CURL* curl = curl_easy_init();

    easy_handles.push_back(curl);
    responses.push_back(HttpResponse {0, string{}});

    curl_easy_setopt(curl, CURLOPT_URL, format("https://www.openstreetmap.org/api/0.6/map?bbox={},{},{},{}", longA, latA, longB, latB).c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &(responses[i].data));

    curl_multi_add_handle(curlm, curl);
  }

  int still_running = 1;
  while(still_running) {
    // update number of running tasks
    CURLMcode mc = curl_multi_perform(curlm, &still_running);
    // fatal error, stop everything and return invalid data
    if (mc) {
      for(HttpResponse& resp : responses) {
        resp.status_code = -1;
      }
      break;
    }

    // poll so the cpu does not explode
    if (still_running) {
      mc = curl_multi_poll(curlm, NULL, 0, 1000, NULL); 
    }

    // read msgs of finished taks
    CURLMsg* msg;
    int _queued;
    do {
      msg = curl_multi_info_read(curlm, &_queued);
      if (!msg || msg->msg != CURLMSG_DONE) continue;

      // find which handle is done
      size_t handle_idx = 0;
      for (size_t i = 0; i < easy_handles.size(); ++i) {
        if (msg->easy_handle == easy_handles[i]) {
          handle_idx = i;
          break;
        }
      }

      // write the response code and kick that guy out
      long* status_code = &(responses[handle_idx].status_code);
      curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, status_code);
      curl_multi_remove_handle(curlm, msg->easy_handle);
      curl_easy_cleanup(msg->easy_handle);
      easy_handles[handle_idx] = nullptr;

    } while(msg);
  }

  curl_multi_cleanup(curlm);

  return responses;
}
