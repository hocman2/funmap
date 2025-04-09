#pragma once
#include <array>
#include <vector>
#include "types/earcut.hpp"
#include "raymath.h"
#include "types/map_data.hpp"

enum class ChunkStatus {Pending, Generating, Generated, Invalid};

using Road = Way;

struct Chunk {
  // Build a chunk with it's internal computed values ready
  Chunk(double longA, double latA, double longB, double latB);

  // south-west point
  const double min_lon = {0};
  const double min_lat = {0};
  // north-east point
  const double max_lon = {0};
  const double max_lat = {0};
  const Vector2 world_min = Vector2Zero();
  const Vector2 world_max = Vector2Zero();
  ChunkStatus status = ChunkStatus::Pending;

  void upload_meshes(std::vector<EarcutMesh>&& meshes);
  void upload_roads(std::vector<Road>&& roads);
  void unload();
  std::array<std::shared_ptr<Chunk>, 8> generate_adjacents() const;
  const std::vector<EarcutMesh>& meshes() const { return m.meshes; }
  const std::vector<Road>& roads() const { return m.roads; }
private:
  struct M {
    std::vector<EarcutMesh> meshes {};
    std::vector<Road> roads {};
  } m;
};
