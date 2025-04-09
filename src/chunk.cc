#include "chunk.hpp"
#include "map_data.hpp"
#include "earcut.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <array>
#include <vector>

using namespace std;

Chunk::Chunk(double longA, double latA, double longB, double latB):
  min_lon(longA), min_lat(latA), max_lon(longB), max_lat(latB),
  world_min(to2DCoords(longA, latA)), world_max(to2DCoords(longB, latB)),
  m()
{}

void Chunk::upload_meshes(vector<EarcutMesh>&& in_meshes) {
  m.meshes = std::move(in_meshes);

  for (EarcutMesh& mesh : m.meshes) {
    UploadMesh(&(mesh.mesh), false);
  }
}

void Chunk::upload_roads(vector<Way>&& in_roads) {
  m.roads = std::move(in_roads);
}

void Chunk::unload() {
  for (EarcutMesh& mesh : m.meshes) 
    UnloadMesh(mesh.mesh);
  m.meshes.clear();
  m.roads.clear();
  status = ChunkStatus::Pending;
}

array<shared_ptr<Chunk>, 8> Chunk::generate_adjacents() const {
  Vector2 delta = Vector2Subtract(world_max, world_min);

  array<shared_ptr<Chunk>, 8> adjacents;

  double longA, latA, longB, latB;
  // north
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x, world_min.y + delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x, world_max.y + delta.y}); 
  adjacents.at(0) = make_shared<Chunk>(longA, latA, longB, latB);

  // north-east
  tie(longA, latA) = toMapCoords(Vector2 {world_max.x, world_min.y + delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x + delta.x, world_max.y + delta.y}); 
  adjacents.at(1) = make_shared<Chunk>(longA, latA, longB, latB);

  // east
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x + delta.x, world_min.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x + delta.x, world_max.y}); 
  adjacents.at(2) = make_shared<Chunk>(longA, latA, longB, latB);

  // south-east
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x + delta.x, world_min.y - delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x + delta.x, world_max.y - delta.y}); 
  adjacents.at(3) = make_shared<Chunk>(longA, latA, longB, latB);

  // south
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x, world_min.y - delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x, world_max.y - delta.y}); 
  adjacents.at(4) = make_shared<Chunk>(longA, latA, longB, latB);

  // south-west
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x - delta.x, world_min.y - delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x - delta.x, world_max.y - delta.y}); 
  adjacents.at(5) = make_shared<Chunk>(longA, latA, longB, latB);

  // west
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x - delta.x, world_min.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x - delta.x, world_max.y}); 
  adjacents.at(6) = make_shared<Chunk>(longA, latA, longB, latB);

  // north-west
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x - delta.x, world_min.y + delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x - delta.x, world_max.y + delta.y}); 
  adjacents.at(7) = make_shared<Chunk>(longA, latA, longB, latB);

  return adjacents;
}
