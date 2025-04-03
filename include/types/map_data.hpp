#pragma once
#include <string_view>
#include <string>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <memory>

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
  // not ideal, it allows tag values and keys to persist past MapData's lifetime tho
  static std::unordered_set<std::string> tag_keys;
  static std::unordered_set<std::string> tag_values;
};

struct HttpResponse {
  std::shared_ptr<class Chunk> target;
  long status_code;
  std::string data;
};
