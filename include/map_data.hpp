#pragma once
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <iostream>
#include <ranges>
#include <memory>
#include <optional>
#include "types/map_data.hpp"
#include "chunk.hpp"


void setProjectionReference(double lon, double lat);
struct Vector2 to2DCoords(double lon, double lat);
std::pair<double, double> toMapCoords(struct Vector2 v);

std::optional<MapData> parse_map_data(std::string_view response);
