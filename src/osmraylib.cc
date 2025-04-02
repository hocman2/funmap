#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "rcamera.h"
#include <print>
#include <iostream>
#include <cmath>
#include <ranges>
#include <future>
#include <expected>
#include <variant>
#include "map_data.hpp"
#include "worker_map_build.hpp"
#include "earcut.hpp"

using namespace std;

// These are the starting coordinates
const double LONG_A = 2.25797;
const double LAT_A  = 48.61416;
const double LONG_B = 2.26037;
const double LAT_B  = 48.61511;
vector<EarcutMesh> meshes;
vector<Way> roads;

void print_vec2(float x, float y) {
  println("Vector2({}, {})", x, y);
}

void print_vec2(Vector2 v) {
  print_vec2(v.x, v.y);
}

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

bool handle_map_build_finished(future<WorkerMapBuild::ExpectedJobResult>& fut) {
  // new results came in from the map build worker
  if (!fut.valid() || fut.wait_for(0ms) != future_status::ready) return false;

  WorkerMapBuild::ExpectedJobResult res = fut.get();

  if (!res) {
    WorkerMapBuild::JobError err = res.error();
    if (auto* http = get_if<WorkerMapBuild::ErrorHttp>(&err)) {
      (void)http;
      // do some stuff with the ror
      return false;
    }
  }

  // first off, meshes must be uploaded to the opengl context, can only be done on the main thread
  for (EarcutMesh& m : res->meshes) {
    UploadMesh(&m.mesh, false);
  }

  meshes.insert_range(meshes.end(), res->meshes);
  roads.insert_range(roads.end(), res->roads);

  return true;
}

int main() {
  InitWindow(1000, 1000, "ZIZIMAP");
  SetTargetFPS(60);
  DisableCursor();

  // The reference point for future projections, if we leave this at 0.0 0.0
  // we'll have heavy distortion because we are using flat projection (tangent plane)
  setProjectionReference(LONG_A, LAT_A);

  double longA = LONG_A;
  double longB = LONG_B;
  double latA = LAT_A;
  double latB = LAT_B;
  Vector2 world_a = to2DCoords(longA, latA);
  Vector2 world_b = to2DCoords(longB, latB);

  WorkerMapBuild build_worker {};
  build_worker.start_idling();

  Camera3D camera = { 
    .position = { 0.0f, 10.0f, 10.0f }, 
    .target = { 0.0f, 0.0f, 0.0f }, 
    .up = { 0.0f, 1.0f, 0.0f }, 
    .fovy = 45.0f, 
    .projection = CAMERA_PERSPECTIVE
  };

  Material mat = initialize_mat();
  vector<pair<Vector2, Vector2>> coordinate_pairs;
  coordinate_pairs.push_back(make_pair(world_a, world_b));

  future<WorkerMapBuild::ExpectedJobResult> map_build_job_future;
  while(!WindowShouldClose()) {
    UpdateCamera(&camera, CAMERA_FREE);

    if (handle_map_build_finished(map_build_job_future)) {
      Vector2 delta = Vector2Subtract(world_b, world_a);
      tie(longA, latA) = toMapCoords(Vector2 {world_a.x - delta.x, world_a.y}); 
      tie(longB, latB) = toMapCoords(Vector2 {world_a.x, world_b.y}); 

      world_a = to2DCoords(longA, latA);
      world_b = to2DCoords(longB, latB);
      coordinate_pairs.push_back(make_pair(world_a, world_b));
    }
    
    if (IsKeyPressed(KEY_R)) {
      WorkerMapBuild::JobParams params;
      params.longA = longA;
      params.latA = latA;
      params.longB = longB;
      params.latB = latB;
      params.promise = promise<WorkerMapBuild::ExpectedJobResult>();

      map_build_job_future = params.promise.get_future();

      build_worker.start_job(std::move(params));
    }

    float cameraPos[3] = {camera.position.x, camera.position.y, camera.position.z};
    SetShaderValue(mat.shader, mat.shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

    BeginDrawing();
      ClearBackground(RAYWHITE);

      BeginMode3D(camera);
        for (const EarcutMesh& m : meshes) {
          Vector2 mesh_pos = m.world_offset;
          Matrix transform = MatrixTranslate(mesh_pos.x, 0.f, mesh_pos.y);
          DrawMesh(m.mesh, mat, transform);
        }

        for (const Way& w : roads) {
          if (w.nodes.size() < 2) continue;
          Vector2 pv = to2DCoords(w.nodes[0].longitude, w.nodes[0].latitude);
          for (size_t i = 1; i < w.nodes.size(); ++i) {
            Vector2 end = to2DCoords(w.nodes[i].longitude, w.nodes[i].latitude);
            DrawLine3D(Vector3 {pv.x, 0.f, pv.y}, Vector3 {end.x, 0.f, end.y}, BLUE);
            pv = end;
          }
        }

        for (const auto& pair : coordinate_pairs) {
          DrawSphere(Vector3(pair.first.x, 0.f, pair.first.y), .5f, RED);
          DrawSphere(Vector3(pair.second.x, 0.f, pair.second.y), .5f, GREEN);
          DrawSphere(Vector3(pair.second.x, 0.f, pair.first.y), .5f, BLUE);
          DrawSphere(Vector3(pair.first.x, 0.f, pair.second.y), .5f, PURPLE);
        }

        DrawGrid(10, 1.f);
      EndMode3D();
    EndDrawing();
  }

  build_worker.end(); 
  UnloadMaterial(mat);
  CloseWindow();
  return 0;
}
