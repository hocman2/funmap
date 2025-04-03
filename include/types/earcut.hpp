#pragma once
#include <tuple>
#include <vector>
#include "raylib.h"

using Triangle = std::tuple<Vector3, Vector3, Vector3>;
struct EarcutResult {
  std::vector<Triangle> triangles;
  Vector2 world_offset;
};

struct EarcutMesh {
  Mesh mesh;
  Vector2 world_offset;
};
