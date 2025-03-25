#include "raylib.h"
#include "tinyxml2.h"
#include <curl/curl.h>
#include <string>
#include <print>
#include <format>
#include <cstdint>
#include <cmath>
#include "map_data.hpp"

using namespace tinyxml2;
using namespace std;

Tag Tag::make_valueless(const MapData& md, const char* key) noexcept { 
  return {
    .key = std::string_view(*(md.tag_keys.find(key))), 
    .value = std::string_view("")
  }; 
}

bool Way::is_building() const noexcept {
  Tag valueless_building = Tag::make_valueless(*(this->md), "building");
  auto building_tag = this->tags.find(valueless_building);
  if (building_tag == this->tags.end())
    return false;
  else if (strcmp(building_tag->value.data(), "yes") == 0)
    return true;
  else
    return false;
}

bool Way::is_highway() const noexcept {
  Tag valueless_highway = Tag::make_valueless(*(this->md), "highway");
  auto highway_tag = this->tags.find(valueless_highway);
  if (highway_tag == this->tags.end())
    return false;
  else
    return true;
}

Vector2 to2DCoords(double lon, double lat) {
  const float scale = 10000.f;
  return Vector2 {
    .x = (float)(lat*scale),
    .y = (float)(lon*scale*cos(lat*M_PI/180.f)),
  };
}

static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  (void)size;

  *((string*)userdata) += string(ptr, nmemb); 
  return nmemb;
}

void fetch_and_parse(MapData* md, double longA, double latA, double longB, double latB) {
  string response_data("");

  CURL* curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, format("https://www.openstreetmap.org/api/0.6/map?bbox={},{},{},{}", longA, latA, longB, latB).c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  curl_easy_perform(curl); 
  curl_easy_cleanup(curl);

  XMLDocument doc;
  doc.Parse(response_data.c_str());

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
        md->nodes.insert({n.id, n});
    } else if (strcmp(elem->Name(), "way") == 0) {
      Way w;
      w.id = elem->Unsigned64Attribute("id");
      w.md = md;
      for (XMLElement* c = elem->FirstChildElement(); c != elem->LastChildElement(); c = c->NextSiblingElement()) {
        if (strcmp(c->Name(), "nd") == 0) {
          const Node& nr = (*(md->nodes.find(c->Unsigned64Attribute("ref")))).second;
          w.nodes.push_back(&nr);
        } else if (strcmp(c->Name(), "tag") == 0) {
          Tag t;
          const char* keyVal = c->FindAttribute("k")->Value();
          string key(keyVal);
          if (!md->tag_keys.contains(key)) {
            md->tag_keys.insert(key);
          }
          t.key = string_view(*(md->tag_keys.find(key)));

          const char* valVal = c->FindAttribute("v")->Value();
          string val(valVal);
          if (!md->tag_values.contains(val)) {
            md->tag_values.insert(val);
          }
          t.value = string_view(*(md->tag_values.find(val)));

          w.tags.insert(t);
        } else {
          println("Unimplemented way child tag: {}", c->Name());
        }
      }
      md->ways.push_back(w);
    } else {
      println("Unimplemented element: {}", elem->Name());
    }
  }
}
