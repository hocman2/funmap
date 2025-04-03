#pragma once
#include <array>
#include <vector>
#include <mutex>
#include <memory>
#include "types/earcut.hpp"
#include "raymath.h"
#include "types/map_data.hpp"

enum class ChunkStatus {Pending, Generating, Generated, Invalid};

class Chunk {
private:
  using UniqueEarcutMesh = std::unique_ptr<EarcutMesh>;
  using UniqueRoad = std::unique_ptr<Way>;
public:
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

  ChunkStatus status() const { 
    const std::lock_guard<std::mutex> lock(m_mtx); 
    return m.status; 
  }

  void status(ChunkStatus new_stat) {
    const std::lock_guard<std::mutex> lock(m_mtx);
    m.status = new_stat;
  }

  template <typename Func>
  requires std::invocable<
    Func, 
    const std::vector<UniqueEarcutMesh>&, 
    const std::vector<UniqueRoad>&
  >
  // bro wtf 😂😂
  void with_data(Func&& fn) const {
    const std::lock_guard<std::mutex> lock(m_mtx);
    fn(m.meshes, m.roads);
  }

  void upload_meshes(std::vector<UniqueEarcutMesh>&& meshes);
  void upload_roads(std::vector<UniqueRoad>&& roads);
  void unload();
  std::array<std::shared_ptr<Chunk>, 8> generate_adjacents() const;
private:
  // we need to protect these field from multi thread access
  struct M {
    std::vector<UniqueEarcutMesh> meshes {};
    std::vector<UniqueRoad> roads {};
    ChunkStatus status = ChunkStatus::Pending;
  } m;
  // must be outside of "m" because it needs mutability in const
  mutable std::mutex m_mtx {};
};
