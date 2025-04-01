#include "earcut.hpp"

#include <cassert>
#include <cmath>
#include <ranges>
#include <vector>
#include "map_data.hpp"
#include "raylib.h"
#include "raymath.h"

using namespace std;
namespace views = ranges::views;

EarcutResult earcut_single(const Way& w) {
  assert(w.nodes.size() >= 3 && "Unimplemented: handle case when building has less than 3 nodes (weird)");
  const size_t LIST_NODES_BUFFER_SZ = 64;
  const float BUILDING_ELEVATION = 0.5f;
  struct ListNode {
    Vector2 data;
    bool is_convex;
    ListNode* nx;
    ListNode* pv;
  };

  // We are not inverting origin.y to keep the world_transform consistent in the return value,
  // we do need to invert it when generating 2D coordinates below
  Vector2 origin = to2DCoords(w.nodes[0].longitude, w.nodes[0].latitude);

  // simply transform node coordinates into Vector2s w/ origin being the first node's coordinates
  auto verts_range = w.nodes 
    | views::take(w.nodes.size()-1)  // skip last node as it's == to the first one
    | views::transform([&origin](const Node& n) -> Vector2 { 
      // it's importaznt to invert the y coordinate because we are ultimately projecting on XZ plane and
      // OGL's Z is inverted
      // Not doing this results in an incorrect mirrored map
      Vector2 v = Vector2Subtract(to2DCoords(n.longitude, n.latitude), Vector2 {origin.x, -origin.y});
      v.y = -v.y;
      return v; 
    });

  // storing vertices data as a doubly linked list
  ListNode vertices_buffer[LIST_NODES_BUFFER_SZ];
  size_t idx = 0;
  for (auto it = verts_range.begin(); it != verts_range.end(); ++it) {
    assert(idx < LIST_NODES_BUFFER_SZ-1 && "The vertices ListNode array is too small");
    vertices_buffer[idx].data = *it;
    vertices_buffer[idx].is_convex = false;

    if (idx > 0) {
      vertices_buffer[idx].pv = &vertices_buffer[idx-1];
      vertices_buffer[idx-1].nx = &vertices_buffer[idx];
    }

    ++idx;
  };

  ListNode* vert_head = &vertices_buffer[0];
  ListNode* vert_tail = &vertices_buffer[idx-1];
  vert_tail->nx = vert_head;
  vert_head->pv = vert_tail;
  const int num_verts = idx;

  // the signed area allows us to know if the winding is clockwise or not, useful for determining vertex concaveness 
  float double_signed_area = 0.0f;
  ListNode* vertex_i = vert_head;
  do {
    ListNode* vertex_nx = vertex_i->nx;
    double_signed_area += (vertex_i->data.x * vertex_nx->data.y - vertex_nx->data.x * vertex_i->data.y);
    vertex_i = vertex_nx;
  } while(vertex_i != vert_head);
  bool winding_clockwise = double_signed_area > 0;

  // keep track of convex vertices
  auto update_convex = [&winding_clockwise](ListNode* vertex) {
    Vector2 vi = vertex->data;
    Vector2 vp = vertex->pv->data;
    Vector2 vn = vertex->nx->data;

    Vector2 a = Vector2Subtract(vp, vi), b = Vector2Subtract(vn, vi);
    float cross = a.x * b.y - a.y * b.x;
    if ((winding_clockwise && cross < 0) || (!winding_clockwise && cross > 0)) {
      vertex->is_convex = true;
    } else {
      vertex->is_convex = false;
    }
  };

  vertex_i = vert_head;
  do {
    update_convex(vertex_i);
    vertex_i = vertex_i->nx;
  } while(vertex_i != vert_head);

  vector<Triangle> triangles;
  triangles.reserve(2*num_verts-2);

  // before performing the actual earcut, we'll generate side faces (walls)
  // we are doing that before earcutting because the process messes up the linked list
  vertex_i = vert_head;
  do {
    if (winding_clockwise) {
      triangles.push_back({
        Vector3 { .x = vertex_i->data.x,      .y = 0.f               , .z = vertex_i->data.y },
        Vector3 { .x = vertex_i->data.x,      .y = BUILDING_ELEVATION, .z = vertex_i->data.y },
        Vector3 { .x = vertex_i->nx->data.x,  .y = BUILDING_ELEVATION, .z = vertex_i->nx->data.y },
      });
      triangles.push_back({
        Vector3 { .x = vertex_i->nx->data.x,  .y = 0.f               , .z = vertex_i->nx->data.y },
        Vector3 { .x = vertex_i->data.x,      .y = 0.f               , .z = vertex_i->data.y },
        Vector3 { .x = vertex_i->nx->data.x,  .y = BUILDING_ELEVATION, .z = vertex_i->nx->data.y },
      });
    } else {
      triangles.push_back({
        Vector3 { .x = vertex_i->data.x,      .y = 0.f               , .z = vertex_i->data.y },
        Vector3 { .x = vertex_i->nx->data.x,  .y = BUILDING_ELEVATION, .z = vertex_i->nx->data.y },
        Vector3 { .x = vertex_i->data.x,      .y = BUILDING_ELEVATION, .z = vertex_i->data.y },
      });
      triangles.push_back({
        Vector3 { .x = vertex_i->nx->data.x,  .y = BUILDING_ELEVATION, .z = vertex_i->nx->data.y },
        Vector3 { .x = vertex_i->data.x,      .y = 0.f               , .z = vertex_i->data.y },
        Vector3 { .x = vertex_i->nx->data.x,  .y = 0.f               , .z = vertex_i->nx->data.y },
      });
    }

    vertex_i = vertex_i->nx;
  } while (vertex_i != vert_head);

  auto push_triangle = [&triangles, &winding_clockwise, &BUILDING_ELEVATION](Vector2& a, Vector2& b, Vector2& c) {
    if (winding_clockwise) {
      triangles.push_back({
        Vector3 { .x = a.x, .y = BUILDING_ELEVATION, .z = a.y },
        Vector3 { .x = c.x, .y = BUILDING_ELEVATION, .z = c.y },
        Vector3 { .x = b.x, .y = BUILDING_ELEVATION, .z = b.y },
      });
    } else {
      triangles.push_back({
        Vector3 { .x = a.x, .y = BUILDING_ELEVATION, .z = a.y },
        Vector3 { .x = b.x, .y = BUILDING_ELEVATION, .z = b.y },
        Vector3 { .x = c.x, .y = BUILDING_ELEVATION, .z = c.y },
      });
    }
  };

  // actual earcutting
  int remaining_verts = num_verts;
  for (ListNode* vertex = vert_head; remaining_verts > 3; vertex = vertex->nx) {
    Vector2 vi = vertex->data;
    Vector2 vp = vertex->pv->data;
    Vector2 vn = vertex->nx->data;

    ListNode* p = vertex->nx->nx;
    bool is_ear = true;
    while(p != vertex->pv) {
      if (!p->is_convex && CheckCollisionPointTriangle(p->data, vi, vp, vn)) {
        is_ear = false;
        break;
      }

      p = p->nx;
    }

    if (!is_ear) {
      if (vertex == vert_tail && remaining_verts == num_verts) 
        assert(false && "0 ear found for a polygon, this should be investigated");
      else continue;
    }

    push_triangle(vp, vi, vn);

    vertex->pv->nx = vertex->nx;
    vertex->nx->pv = vertex->pv;
    --remaining_verts;

    if (vertex == vert_head) {
      vert_head = vertex->pv; 
    } else if (vertex == vert_tail) {
      vert_tail = vertex->nx;
    }

    // convexity must be recalculated 
    update_convex(vertex->pv);
    update_convex(vertex->nx);
  }

  // push the remaining triangle
  push_triangle(vert_head->pv->data, vert_head->data, vert_head->nx->data);

  return EarcutResult { 
    .triangles = triangles,
    .world_offset = origin 
  };
}

vector<EarcutMesh> build_meshes(const vector<EarcutResult>& earcuts) {
  auto build_and_upload_single = [](const EarcutResult& earcut) -> EarcutMesh {
    Mesh mesh {0};
    size_t num_tris = earcut.triangles.size();
    mesh.vertexCount = num_tris * 3;
    mesh.triangleCount = num_tris;
    mesh.vertices = (float*)RL_MALLOC(mesh.vertexCount*3*sizeof(float));
    mesh.normals = (float*)RL_MALLOC(mesh.vertexCount*3*sizeof(float));

    for (int i = 0; i < mesh.triangleCount; ++i) {
      const Triangle& tri = earcut.triangles[i];

      Vector3 v1 = get<0>(tri);
      mesh.vertices[i*9+0] = v1.x;
      mesh.vertices[i*9+1] = v1.y;
      mesh.vertices[i*9+2] = v1.z;

      Vector3 v2 = get<1>(tri);
      mesh.vertices[i*9+3] = v2.x;
      mesh.vertices[i*9+4] = v2.y;
      mesh.vertices[i*9+5] = v2.z;

      Vector3 v3 = get<2>(tri);
      mesh.vertices[i*9+6] = v3.x;
      mesh.vertices[i*9+7] = v3.y;
      mesh.vertices[i*9+8] = v3.z;

      Vector3 normal = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(v2, v1), Vector3Subtract(v3, v1)));

      mesh.normals[i*9+0] = normal.x;
      mesh.normals[i*9+1] = normal.y;
      mesh.normals[i*9+2] = normal.z;

      mesh.normals[i*9+3] = normal.x;
      mesh.normals[i*9+4] = normal.y;
      mesh.normals[i*9+5] = normal.z;

      mesh.normals[i*9+6] = normal.x;
      mesh.normals[i*9+7] = normal.y;
      mesh.normals[i*9+8] = normal.z;
    }

    return EarcutMesh {
      .mesh = mesh,
      .world_offset = earcut.world_offset,
    };
  };

  auto mesh_transform = earcuts | views::transform(build_and_upload_single);
  return vector<EarcutMesh>(mesh_transform.begin(), mesh_transform.end());
}
