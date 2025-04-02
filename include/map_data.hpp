#pragma once

#include <vector>
#include "tinyxml2.h"
#include <unordered_set>
#include <unordered_map>
#include <string_view>
#include <string>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <optional>
#include <ranges>
#include "chunk.hpp"

struct Node {
  uint64_t id;
  double longitude;
  double latitude;
  bool visible;
};

struct Tag {
  std::string_view key;
  std::string_view value;

  // forced to implement this like that, it's used to find tags by key when using map::find or set::find. maybe the stl allows us to use a custom compare fn ? too lazy to check
  bool operator==(const Tag& other) const noexcept { return key == other.key; }
  static Tag make_valueless(const char* key) noexcept;
};

template <>
struct std::hash<Tag> {
  std::size_t operator()(const Tag& t) const noexcept {
    return std::hash<std::string_view>{}(t.key);
  } 
};

struct Way {
  uint64_t id;
  // data here may be duplicated but it allows Ways to exist outside of MapData since they are used to describe roads
  std::vector<Node> nodes;
  std::unordered_set<Tag> tags;
  
  bool is_building() const noexcept;
  bool is_highway() const noexcept;
};

struct MapData {
  MapData(): nodes(), ways() {}
  std::unordered_map<uint64_t, Node> nodes;
  std::vector<Way> ways;
  static std::unordered_set<std::string> tag_keys;
  static std::unordered_set<std::string> tag_values;
};

void setProjectionReference(double lon, double lat);
struct Vector2 to2DCoords(double lon, double lat);
std::pair<double, double> toMapCoords(struct Vector2 v);

struct HttpResponse {
  long status_code;
  std::string data;
};

template <typename Pred>
using XmlResponsesFilterView = std::ranges::transform_view<std::ranges::ref_view<std::vector<HttpResponse>>, Pred>;

std::vector<HttpResponse> fetch_map_data(const std::vector<Chunk>& chunks);

template <typename Pred>
std::vector<MapData> parse_map_data(XmlResponsesFilterView<Pred> xml_responses) {
  using namespace std;
  using namespace tinyxml2;

  vector<MapData> mds;
  mds.reserve(xml_responses.size());

  size_t i = 0;
  for (const auto& response : xml_responses) {
    mds.push_back(MapData {});
    MapData& md = mds[i];

    if (!response.has_value()) continue;

    XMLDocument doc;
    doc.Parse(response->data());
    XMLNode* lastChild = doc.RootElement()->LastChildElement();

    for (XMLElement* elem = doc.RootElement()->FirstChildElement(); elem != lastChild; elem = elem->NextSiblingElement()) {
      if (elem == nullptr) {
        break;
      }

      XMLPrinter printer;
      elem->Accept(&printer);

      if (strcmp(elem->Name(), "node") == 0) {
          Node n;
          n.id = elem->Unsigned64Attribute("id");
          n.longitude = elem->DoubleAttribute("lon");
          n.latitude = elem->DoubleAttribute("lat");
          n.visible = elem->BoolAttribute("visible");
          md.nodes.insert({n.id, n});
      } else if (strcmp(elem->Name(), "way") == 0) {
        Way w;
        w.id = elem->Unsigned64Attribute("id");
        for (XMLElement* c = elem->FirstChildElement(); c != elem->LastChildElement(); c = c->NextSiblingElement()) {
          if (strcmp(c->Name(), "nd") == 0) {
            const Node& nr = (*(md.nodes.find(c->Unsigned64Attribute("ref")))).second;
            w.nodes.push_back(nr);
          } else if (strcmp(c->Name(), "tag") == 0) {
            Tag t;
            const char* keyVal = c->FindAttribute("k")->Value();
            string key(keyVal);
            if (!MapData::tag_keys.contains(key)) {
              md.tag_keys.insert(key);
            }
            t.key = string_view(*(MapData::tag_keys.find(key)));

            const char* valVal = c->FindAttribute("v")->Value();
            string val(valVal);
            if (!MapData::tag_values.contains(val)) {
              md.tag_values.insert(val);
            }
            t.value = string_view(*(MapData::tag_values.find(val)));

            w.tags.insert(t);
          } else {
            println("Unimplemented way child tag: {}", c->Name());
          }
        }
        md.ways.push_back(w);
      } else {
        println("Unimplemented element: {}", elem->Name());
      }
    }
    ++i;
  }

  return mds;
}
