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
#include <array>
#include <variant>
#include "map_data.hpp"
#include "worker_map_build.hpp"
#include "earcut.hpp"
#include "chunk.hpp"

using namespace std;

// These are the starting coordinates
const double LONG_A = 2.25797;
const double LAT_A  = 48.61416;
const double LONG_B = 2.26037;
const double LAT_B  = 48.61511;
vector<Chunk> chunks;
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

bool handle_map_build_finished(future<vector<WorkerMapBuild::ExpectedJobResult>>& fut) {
  // new results came in from the map build worker
  if (!fut.valid() || fut.wait_for(0ms) != future_status::ready) return false;

  size_t i = 0;
  vector<WorkerMapBuild::ExpectedJobResult> results = fut.get();
  for (auto& res : results) {
    if (!res) {
      chunks[i].status = ChunkStatus::Invalid;
      WorkerMapBuild::JobError err = res.error();
      if (auto* internal = get_if<WorkerMapBuild::ErrorInternal>(&err)) {
        (void)internal;
        // do some stuff with the error, everything failed anyway
        return false;
      }
      if (auto* http = get_if<WorkerMapBuild::ErrorHttp>(&err)) {
        (void)http;
        // do some stuff with the ror
      }
    }

    // first off, meshes must be uploaded to the opengl context, can only be done on the main thread
    for (EarcutMesh& m : res->meshes) {
      UploadMesh(&m.mesh, false);
    }

    chunks[i].status = ChunkStatus::Generated;
    meshes.insert_range(meshes.end(), res->meshes);
    roads.insert_range(roads.end(), res->roads);
    ++i;
  }

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
  chunks.push_back(Chunk::build(longA, latA, longB, latB));

  future<vector<WorkerMapBuild::ExpectedJobResult>> map_build_job_future;
  while(!WindowShouldClose()) {
    UpdateCamera(&camera, CAMERA_FREE);

    if (handle_map_build_finished(map_build_job_future)) {
      if (chunks.size() == 1) {
        auto adjacents = chunks[0].generate_adjacents();
        chunks.insert(chunks.end(), adjacents.begin(), adjacents.end());
      }
    }
    
    if (IsKeyPressed(KEY_R)) {
      for (Chunk& c : chunks) c.status = ChunkStatus::Generating;

      WorkerMapBuild::JobParams params;
      params.chunks = chunks;
      params.promise = promise<vector<WorkerMapBuild::ExpectedJobResult>>();

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

        for (const Chunk& chunk : chunks) {
          DrawSphere(Vector3(chunk.world_min.x, 0.f, chunk.world_min.y), .25f, Fade(RED, 0.5f));
          DrawSphere(Vector3(chunk.world_max.x, 0.f, chunk.world_max.y), .25f, Fade(GREEN, 0.5f));
          DrawSphere(Vector3(chunk.world_max.x, 0.f, chunk.world_min.y), .25f, Fade(BLUE, 0.5f));
          DrawSphere(Vector3(chunk.world_min.x, 0.f, chunk.world_max.y), .25f, Fade(PURPLE, 0.5f));

          Color plane_color;
          switch(chunk.status) {
            case ChunkStatus::Pending:
            plane_color = Fade(SKYBLUE, 0.33f);
            break;
            case ChunkStatus::Generating:
            plane_color = Fade(ORANGE, 0.33f);
            break;
            case ChunkStatus::Generated:
            plane_color = Fade(GREEN, 0.33f);
            break;
            case ChunkStatus::Invalid:
            plane_color = Fade(RED, 0.33f);
            break;
            default:
            plane_color = BLACK;
            break;
          };

          Vector2 size = Vector2Subtract(chunk.world_max, chunk.world_min);
          size.x = abs(size.x);
          size.y = abs(size.y);
          DrawPlane(Vector3(chunk.world_min.x + size.x * 0.5f, 0.f, chunk.world_min.y - size.y * 0.5f), size, plane_color);
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
