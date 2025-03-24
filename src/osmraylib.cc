#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include <print>
#include <iostream>
#include <cmath>
#include <ranges>
#include "map_data.hpp"
#include "earcut.hpp"

using namespace std;

const double LONG_A = 2.25797;
const double LAT_A  = 48.61416;
const double LONG_B = 2.26037;
const double LAT_B  = 48.61511;

int main(void) {
  InitWindow(1000, 1000, "ZIZIMAP");
  SetTargetFPS(60);
  DisableCursor();

  MapData md;
  fetch_and_parse(&md, LONG_A, LAT_A, LONG_B, LAT_B);
  Vector2 world_origin = to2DCoords(LONG_A, LAT_A);

  println("Num nodes: {}", md.nodes.size());
  println("Num ways: {}", md.ways.size());

  vector<EarcutMesh> meshes = [&md]() {
    auto building_ways = md.ways | ranges::views::filter([](const Way& w){ return w.is_building(); });
    vector<EarcutResult> earcuts = earcut_collection(std::move(building_ways));
    return build_and_upload_meshes(earcuts);
  }();

  auto road_ways_view = md.ways | ranges::views::filter([](const Way& w) { return w.is_highway(); });
  vector<Way> road_ways(road_ways_view.begin(), road_ways_view.end());

  Camera3D camera = { 
    .position = { 0.0f, 10.0f, 10.0f }, 
    .target = { 0.0f, 0.0f, 0.0f }, 
    .up = { 0.0f, 1.0f, 0.0f }, 
    .fovy = 45.0f, 
    .projection = CAMERA_PERSPECTIVE
  };

  Material mat = LoadMaterialDefault();
  mat.maps[0].color = RED;
  while(!WindowShouldClose()) {
    UpdateCamera(&camera, CAMERA_FREE);

    BeginDrawing();
      ClearBackground(RAYWHITE);

      BeginMode3D(camera);
        for (const EarcutMesh& m : meshes) {
          Vector2 mesh_pos = Vector2Subtract(m.world_offset, world_origin);
          Matrix transform = MatrixTranslate(mesh_pos.x, 0.f, mesh_pos.y);
          DrawMesh(m.mesh, mat, transform);
        }

        for (const Way& w : road_ways) {
          if (w.nodes.size() < 2) continue;
          Vector2 pv = Vector2Subtract(to2DCoords(w.nodes[0]->longitude, w.nodes[0]->latitude), world_origin);
          for (size_t i = 1; i < w.nodes.size(); ++i) {
            Vector2 end = Vector2Subtract(to2DCoords(w.nodes[i]->longitude, w.nodes[i]->latitude), world_origin);
            DrawLine3D(Vector3 {pv.x, 0.f, pv.y}, Vector3 {end.x, 0.f, end.y}, BLUE);
            pv = end;
          }
        }
        DrawGrid(10, 1.f);
      EndMode3D();
    EndDrawing();
  }
  
  UnloadMaterial(mat);
  CloseWindow();
  return 0;
}
