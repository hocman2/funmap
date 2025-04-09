#include "map_data.hpp"
#include "raylib.h"
#include "tinyxml2.h"
#include <curl/curl.h>
#include <string>
#include <print>
#include <format>
#include <cstdint>
#include <cassert>
#include <tuple>
#include <memory>
#include <cmath>
#include <optional>

using namespace std;
using namespace tinyxml2;

unordered_set<string> MapData::tag_keys;
unordered_set<string> MapData::tag_values;

Tag Tag::make_valueless(const char* key) noexcept { 
  // trash workaround
  if (!MapData::tag_keys.contains(key)) {
    MapData::tag_keys.insert(key);
  }

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

optional<MapData> parse_map_data(string_view response) {
  MapData md = MapData {};

  XMLDocument doc;
  XMLError parse_res = doc.Parse(response.data());
  if (parse_res != XML_SUCCESS) {
    TraceLog(LOG_ERROR, "XML: Parse error: %s", doc.ErrorStr());
    return std::nullopt;
  }
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

  return md;
}
