#include "chunk.hpp"
#include "map_data.hpp"
#include <array>
#include "raymath.h"

using namespace std;

Chunk Chunk::build(double longA, double latA, double longB, double latB) {
  return Chunk {
    .min_lon = longA, 
    .min_lat = latA,
    .max_lon = longB, 
    .max_lat = latB,
    .world_min = to2DCoords(longA, latA),
    .world_max = to2DCoords(longB, latB),
    .status = ChunkStatus::Pending,
  };
}

array<Chunk, 8> Chunk::generate_adjacents() const {
  Vector2 delta = Vector2Subtract(world_max, world_min);

  array<Chunk, 8> adjacents;

  double longA, latA, longB, latB;
  // north
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x, world_min.y + delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x, world_max.y + delta.y}); 
  adjacents.at(0) = Chunk::build(longA, latA, longB, latB);

  // north-east
  tie(longA, latA) = toMapCoords(Vector2 {world_max.x, world_min.y + delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x + delta.x, world_max.y + delta.y}); 
  adjacents.at(1) = Chunk::build(longA, latA, longB, latB);

  // east
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x + delta.x, world_min.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x + delta.x, world_max.y}); 
  adjacents.at(2) = Chunk::build(longA, latA, longB, latB);

  // south-east
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x + delta.x, world_min.y - delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x + delta.x, world_max.y - delta.y}); 
  adjacents.at(3) = Chunk::build(longA, latA, longB, latB);

  // south
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x, world_min.y - delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x, world_max.y - delta.y}); 
  adjacents.at(4) = Chunk::build(longA, latA, longB, latB);

  // south-west
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x - delta.x, world_min.y - delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x - delta.x, world_max.y - delta.y}); 
  adjacents.at(5) = Chunk::build(longA, latA, longB, latB);

  // west
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x - delta.x, world_min.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x - delta.x, world_max.y}); 
  adjacents.at(6) = Chunk::build(longA, latA, longB, latB);

  // north-west
  tie(longA, latA) = toMapCoords(Vector2 {world_min.x - delta.x, world_min.y + delta.y}); 
  tie(longB, latB) = toMapCoords(Vector2 {world_max.x - delta.x, world_max.y + delta.y}); 
  adjacents.at(7) = Chunk::build(longA, latA, longB, latB);

  return adjacents;
}
