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
#include <memory>
#include <variant>
#include "map_data.hpp"
#include "earcut.hpp"
#include "chunk.hpp"
#include "map_build_job.hpp"

using namespace std;

// These are the starting coordinates
const double LONG_A = 2.25797;
const double LAT_A  = 48.61416;
const double LONG_B = 2.26037;
const double LAT_B  = 48.61511;
shared_ptr<Chunk> start_chunk;
vector<shared_ptr<Chunk>> chunks;

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

void poll_build_job_results(MapBuildJob& build_job) {
  queue<MapBuildJob::ExpectedJobResult> results = build_job.poll();

  while(!results.empty()) {
    MapBuildJob::ExpectedJobResult& res = results.front();

    // handle potential error
    if (!res.result.has_value()) {
      res.target->status = ChunkStatus::Invalid;
      MapBuildJob::JobError err = res.result.error();
      if (auto* internal = get_if<MapBuildJob::ErrorInternal>(&err)) {
        (void)internal;
      } else
      if (auto* http = get_if<MapBuildJob::ErrorHttp>(&err)) {
        (void)http;
      }
    } else {
      res.target->upload_roads(std::move(res.result->roads));
      res.target->upload_meshes(std::move(res.result->meshes));
      res.target->status = ChunkStatus::Generated;
    }


    results.pop();
  }
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

  Camera3D camera = { 
    .position = { 0.0f, 10.0f, 10.0f }, 
    .target = { 0.0f, 0.0f, 0.0f }, 
    .up = { 0.0f, 1.0f, 0.0f }, 
    .fovy = 45.0f, 
    .projection = CAMERA_PERSPECTIVE
  };

  Material mat = initialize_mat();
  MapBuildJob build_job {};
  bool unload_chunks_next_press = false;
  start_chunk = make_shared<Chunk>(longA, latA, longB, latB);
  chunks.push_back(start_chunk);

  while(!WindowShouldClose()) {
    UpdateCamera(&camera, CAMERA_FREE);

    poll_build_job_results(build_job);
    if (build_job.just_finished()) {
      if (chunks.size() == 1) {
        auto adjacents = start_chunk->generate_adjacents();
        chunks.insert(chunks.end(), adjacents.begin(), adjacents.end());
      } else if (chunks.size() > 1) {
        unload_chunks_next_press = true;
      }
    }
    
    if (IsKeyPressed(KEY_R)) {
      if (unload_chunks_next_press) {
        unload_chunks_next_press = false;
        for (auto& c : chunks)
          c->unload();

        chunks.clear();
        chunks.push_back(start_chunk);
      } else if (!build_job.ongoing()){
        build_job.start(chunks);
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
          for (const EarcutMesh& m : chunk->meshes()) {
            ++num_meshes;
            Vector2 mesh_pos = m.world_offset;
            Matrix transform = MatrixTranslate(mesh_pos.x, 0.f, mesh_pos.y);
            DrawMesh(m.mesh, mat, transform);
          }

          for (const Way& w : chunk->roads()) {
            if (w.nodes.size() < 2) continue;
            ++num_roads;
            Vector2 pv = to2DCoords(w.nodes[0].longitude, w.nodes[0].latitude);
            for (size_t i = 1; i < w.nodes.size(); ++i) {
              Vector2 end = to2DCoords(w.nodes[i].longitude, w.nodes[i].latitude);
              DrawLine3D(Vector3 {pv.x, 0.f, pv.y}, Vector3 {end.x, 0.f, end.y}, BLUE);
              pv = end;
            }
          }

          DrawSphere(Vector3(chunk->world_min.x, 0.f, chunk->world_min.y), .25f, Fade(RED, 0.5f));
          DrawSphere(Vector3(chunk->world_max.x, 0.f, chunk->world_max.y), .25f, Fade(GREEN, 0.5f));
          DrawSphere(Vector3(chunk->world_max.x, 0.f, chunk->world_min.y), .25f, Fade(BLUE, 0.5f));
          DrawSphere(Vector3(chunk->world_min.x, 0.f, chunk->world_max.y), .25f, Fade(PURPLE, 0.5f));

          Color plane_color;
          switch(chunk->status) {
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

  UnloadMaterial(mat);
  CloseWindow();
  return 0;
}
