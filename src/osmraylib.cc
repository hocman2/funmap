#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "rcamera.h"
#include <print>
#include <iostream>
#include <cmath>
#include <ranges>
#include <queue>
#include <expected>
#include <array>
#include <variant>
#include <memory>
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
vector<shared_ptr<Chunk>> chunks;

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

bool handle_map_build_results(WorkerMapBuild& worker) {
  if (!worker.has_job() && !worker.has_results()) return false;
  queue<WorkerMapBuild::ExpectedJobResult> results = worker.take_results();

  while(!results.empty()) {
    WorkerMapBuild::ExpectedJobResult& res = results.front();

    // handle potential error
    if (!res.result) {
      WorkerMapBuild::JobError err = res.result.error();
      if (auto* internal = get_if<WorkerMapBuild::ErrorInternal>(&err)) {
        (void)internal;
        return false;
      } else
      if (auto* http = get_if<WorkerMapBuild::ErrorHttp>(&err)) {
        (void)http;
      }
    }

    res.target->upload_roads(std::move(res.result->roads));
    res.target->upload_meshes(std::move(res.result->meshes));
    res.target->status(ChunkStatus::Generated);

    results.pop();
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
  chunks.push_back(make_shared<Chunk>(longA, latA, longB, latB));

  bool all_chunks_loaded = false;
  while(!WindowShouldClose()) {
    UpdateCamera(&camera, CAMERA_FREE);

    if (handle_map_build_results(build_worker) && !build_worker.has_job()) {
      if (chunks.size() == 1) {
        auto adjacents = chunks[0]->generate_adjacents();
        chunks.insert(chunks.end(), adjacents.begin(), adjacents.end());
      } else {
        all_chunks_loaded = true;
      }
    }
    
    if (IsKeyPressed(KEY_R)) {
      if (all_chunks_loaded) {
        for (size_t i = 0; i < chunks.size(); ++i)
          chunks[i]->unload();

        chunks.erase(chunks.begin()+1, chunks.end());
        all_chunks_loaded = false;
      } else {
        WorkerMapBuild::JobParams params;
        if (chunks.size() == 1)
          params.chunks = chunks;
        else
          params.chunks = vector<shared_ptr<Chunk>>(chunks.begin()+1, chunks.end());
        build_worker.start_job(std::move(params));
      }
    }

    float cameraPos[3] = {camera.position.x, camera.position.y, camera.position.z};
    SetShaderValue(mat.shader, mat.shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

    BeginDrawing();
      ClearBackground(RAYWHITE);

      BeginMode3D(camera);
        int num_chunks_loaded = 0;
        int num_meshes = 0;
        int num_roads = 0;
        for (const auto& chunk : chunks) {
          chunk->with_data([&mat, &num_meshes, &num_roads](const vector<unique_ptr<EarcutMesh>>& meshes, const vector<unique_ptr<Way>>& roads) {
            for (const unique_ptr<EarcutMesh>& m : meshes) {
              ++num_meshes;
              Vector2 mesh_pos = m->world_offset;
              Matrix transform = MatrixTranslate(mesh_pos.x, 0.f, mesh_pos.y);
              DrawMesh(m->mesh, mat, transform);
            }

            for (const unique_ptr<Way>& w : roads) {
              if (w->nodes.size() < 2) continue;
              ++num_roads;
              Vector2 pv = to2DCoords(w->nodes[0].longitude, w->nodes[0].latitude);
              for (size_t i = 1; i < w->nodes.size(); ++i) {
                Vector2 end = to2DCoords(w->nodes[i].longitude, w->nodes[i].latitude);
                DrawLine3D(Vector3 {pv.x, 0.f, pv.y}, Vector3 {end.x, 0.f, end.y}, BLUE);
                pv = end;
              }
            }
          });

          DrawSphere(Vector3(chunk->world_min.x, 0.f, chunk->world_min.y), .25f, Fade(RED, 0.5f));
          DrawSphere(Vector3(chunk->world_max.x, 0.f, chunk->world_max.y), .25f, Fade(GREEN, 0.5f));
          DrawSphere(Vector3(chunk->world_max.x, 0.f, chunk->world_min.y), .25f, Fade(BLUE, 0.5f));
          DrawSphere(Vector3(chunk->world_min.x, 0.f, chunk->world_max.y), .25f, Fade(PURPLE, 0.5f));

          Color plane_color;
          switch(chunk->status()) {
            case ChunkStatus::Pending:
            plane_color = Fade(SKYBLUE, 0.33f);
            break;
            case ChunkStatus::Generating:
            plane_color = Fade(ORANGE, 0.33f);
            break;
            case ChunkStatus::Generated:
            plane_color = Fade(GREEN, 0.33f);
            ++num_chunks_loaded;
            break;
            case ChunkStatus::Invalid:
            plane_color = Fade(RED, 0.33f);
            break;
            default:
            plane_color = BLACK;
            break;
          };

          Vector2 size = Vector2Subtract(chunk->world_max, chunk->world_min);
          size.x = abs(size.x);
          size.y = abs(size.y);
          DrawPlane(Vector3(chunk->world_min.x + size.x * 0.5f, 0.f, chunk->world_min.y - size.y * 0.5f), size, plane_color);
        }

        DrawGrid(10, 1.f);
      EndMode3D();
      DrawFPS(10, 10);
      DrawText(format("{} chunks loaded", num_chunks_loaded).c_str(), 10, 35, 20, BLUE);
      DrawText(format("- {} meshes", num_meshes).c_str(), 15, 55, 18, BLUE);
      DrawText(format("- {} roads", num_roads).c_str(), 15, 73, 18, BLUE);
    EndDrawing();
  }

  build_worker.end(); 
  UnloadMaterial(mat);
  CloseWindow();
  return 0;
}
