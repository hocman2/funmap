#pragma once
#include <ranges>
#include <vector>
#include <tuple>
#include <memory>
#include "raylib.h"
#include "types/earcut.hpp"
#include "types/map_data.hpp"

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
