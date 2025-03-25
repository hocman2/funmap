#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "rcamera.h"
#include <print>
#include <iostream>
#include <cmath>
#include <ranges>
#include "map_data.hpp"
#include "earcut.hpp"

using namespace std;
namespace views = ranges::views;

const double LONG_A = 2.25797;
const double LAT_A  = 48.61416;
const double LONG_B = 2.26037;
const double LAT_B  = 48.61511;

Material initialize_mat() {
  Shader shader = LoadShader("resources/shaders/flat_shade.vs", "resources/shaders/flat_shade.fs");

  Material mat = Material {
    .shader = shader,
    .maps = (MaterialMap*)RL_CALLOC(12, sizeof(MaterialMap)),
  };
  mat.maps[MATERIAL_MAP_ALBEDO].color = DARKPURPLE;

  mat.shader.locs[SHADER_LOC_MATRIX_VIEW] = GetShaderLocation(mat.shader, "matView");
  mat.shader.locs[SHADER_LOC_MATRIX_PROJECTION] = GetShaderLocation(mat.shader, "matProjection");
  mat.shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(mat.shader, "matModel");
  mat.shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(mat.shader, "matNormal");

  mat.shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(mat.shader, "viewPos");
  mat.shader.locs[SHADER_LOC_MAP_ALBEDO] = GetShaderLocation(mat.shader, "albedoColor");

  Vector4 albedo_norm = ColorNormalize(mat.maps[MATERIAL_MAP_ALBEDO].color);
  SetShaderValue(
    mat.shader, 
    mat.shader.locs[SHADER_LOC_MAP_ALBEDO], 
    &albedo_norm,
    SHADER_UNIFORM_VEC4
  );

  return mat;
}

int main(void) {
  InitWindow(1000, 1000, "ZIZIMAP");
  SetTargetFPS(60);
  DisableCursor();

  MapData md;
  fetch_and_parse(&md, LONG_A, LAT_A, LONG_B, LAT_B);
  Vector2 world_origin = to2DCoords(LONG_A, LAT_A);

  println("Num nodes: {}", md.nodes.size());
  println("Num ways: {}", md.ways.size());

  // because it might not be immediatly clear,
  // this expression selects buildings, earcuts each one into a list of triangles
  // then meshes are generated from these triangles
  vector<EarcutMesh> meshes = [&md]() {
    auto buildings = md.ways | views::filter([](const Way& w){ return w.is_building(); });
    vector<EarcutResult> earcuts = earcut_collection(std::move(buildings));
    return build_and_upload_meshes(earcuts);
  }();

  auto roads_view = md.ways | views::filter([](const Way& w) { return w.is_highway(); });
  vector<Way> roads(roads_view.begin(), roads_view.end());

  Camera3D camera = { 
    .position = { 0.0f, 10.0f, 10.0f }, 
    .target = { 0.0f, 0.0f, 0.0f }, 
    .up = { 0.0f, 1.0f, 0.0f }, 
    .fovy = 45.0f, 
    .projection = CAMERA_PERSPECTIVE
  };

  Material mat = initialize_mat();

  while(!WindowShouldClose()) {
    UpdateCamera(&camera, CAMERA_FREE);

    float cameraPos[3] = {camera.position.x, camera.position.y, camera.position.z};
    SetShaderValue(mat.shader, mat.shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

    BeginDrawing();
      ClearBackground(RAYWHITE);

      BeginMode3D(camera);
        for (const EarcutMesh& m : meshes) {
          Vector2 mesh_pos = Vector2Subtract(m.world_offset, world_origin);
          Matrix transform = MatrixTranslate(mesh_pos.x, 0.f, mesh_pos.y);
          DrawMesh(m.mesh, mat, transform);
        }

        for (const Way& w : roads) {
          if (w.nodes.size() < 2) continue;
          Vector2 pv = Vector2Subtract(to2DCoords(w.nodes[0]->longitude, w.nodes[0]->latitude), world_origin);
          for (size_t i = 1; i < w.nodes.size(); ++i) {
            Vector2 end = Vector2Subtract(to2DCoords(w.nodes[i]->longitude, w.nodes[i]->latitude), world_origin);
            DrawLine3D(Vector3 {pv.x, 0.f, pv.y}, Vector3 {end.x, 0.f, end.y}, BLUE);
            pv = end;
          }
        }
        DrawSphere(Vector3Zero(), .5f, RED);
        DrawGrid(10, 1.f);
      EndMode3D();
    EndDrawing();
  }
  
  UnloadMaterial(mat);
  CloseWindow();
  return 0;
}
