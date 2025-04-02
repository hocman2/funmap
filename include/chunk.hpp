#pragma once
#include <array>
#include "raymath.h"

enum class ChunkStatus {Pending, Generating, Generated, Invalid};

struct Chunk {
  // Build a chunk with it's internal computed values ready
  static Chunk build(double longA, double latA, double longB, double latB);

  // south-west point
  double min_lon = {0};
  double min_lat = {0};
  // north-east point
  double max_lon = {0};
  double max_lat = {0};

  Vector2 world_min = Vector2Zero();
  Vector2 world_max = Vector2Zero();

  ChunkStatus status = ChunkStatus::Pending;

  std::array<Chunk, 8> generate_adjacents() const;
};
