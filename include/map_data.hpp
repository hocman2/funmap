#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string_view>
#include <cstdint>
#include <iostream>

struct Node {
  uint64_t id;
  double longitude;
  double latitude;
  bool visible;
};

struct Tag {
  std::string_view key;
  std::string_view value;

  static Tag make_valueless(const struct MapData& md, const char* key) noexcept;

  bool operator==(const Tag& other) const noexcept { return key == other.key; }
};

template <>
struct std::hash<Tag> {
  std::size_t operator()(const Tag& t) const noexcept {
    return std::hash<std::string_view>{}(t.key);
  } 
};

struct Way {
  uint64_t id;
  // ptr to owner for convenience
  const MapData* md;
  std::vector<const Node*> nodes;
  std::unordered_set<Tag> tags;
  
  bool is_building() const noexcept;
  bool is_highway() const noexcept;
};

struct MapData {
  std::unordered_map<uint64_t, Node> nodes;
  std::vector<Way> ways;
  std::unordered_set<std::string> tag_keys;
  std::unordered_set<std::string> tag_values;

  MapData(): nodes(), ways(), tag_keys(), tag_values() {}
};

void fetch_and_parse(MapData* md, double longA, double latA, double longB, double latB);

struct Vector3 to3DCoords(double lon, double lat); 
struct Vector2 to2DCoords(double lon, double lat);
