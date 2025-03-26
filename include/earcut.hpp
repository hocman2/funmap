#pragma once
#include "raylib.h"
#include "map_data.hpp"
#include <ranges>
#include <vector>
#include <tuple>

using Triangle = std::tuple<Vector3, Vector3, Vector3>;
struct EarcutResult {
  std::vector<Triangle> triangles;
  Vector2 world_offset;
};

struct EarcutMesh {
  Mesh mesh;
  Vector2 world_offset;
};

EarcutResult earcut_single(const Way& w);

template <typename Pred>
using WayFilterView = std::ranges::filter_view<std::ranges::ref_view<std::vector<Way>>, Pred>;

template <typename Pred>
std::vector<EarcutResult> earcut_collection(WayFilterView<Pred>&& buildings) {
  std::vector<EarcutResult> earcuts;

  for (const Way& w : buildings) {
    earcuts.push_back(earcut_single(w));
  }

  return earcuts;
}

std::vector<EarcutMesh> build_meshes(const std::vector<EarcutResult>& earcuts);
