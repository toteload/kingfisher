#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#define FAST_OBJ_IMPLEMENTATION
#include <fast_obj.h>
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#include <ufbx.h>
#pragma clang diagnostic pop

#include "model.h"

bool read_obj_triangles(char const *filename, Triangle **triangles, u64 *triangle_count) {
  fastObjMesh *mesh = fast_obj_read(filename);

  if (!mesh) {
    return false;
  }

  u64 count = 0;
  for (u32 i = 0; i < mesh->group_count; i++) {
    fastObjGroup *group = &mesh->groups[i];

    for (u32 j = 0; j < group->face_count; j++) {
      // Only support triangles
      assert(mesh->face_vertices[group->face_offset + j] == 3);
    }

    count += group->face_count;
  }

  Triangle *tris = malloc(count * sizeof(Triangle));

  u64 triangle_offset = 0;
  for (u32 i = 0; i < mesh->group_count; i++) {
    fastObjGroup *group = &mesh->groups[i];

    for (u32 j = 0; j < group->face_count; j++) {
      for (u32 k = 0; k < 3; k++) {
        fastObjIndex idx = mesh->indices[group->index_offset + 3 * j + k];

        assert(idx.p != 0);

        tris[triangle_offset + j].p[k] = (vec3s){{
          mesh->positions[3 * idx.p + 0],
          mesh->positions[3 * idx.p + 1],
          mesh->positions[3 * idx.p + 2],
        }};
      }
    }

    triangle_offset += group->face_count;
  }

  *triangle_count = count;
  *triangles = tris;

  fast_obj_destroy(mesh);

  return true;
}

bool read_fbx_triangles(char const *filename, Triangle **triangles, u64 *triangle_count) {
  ufbx_error err;
  ufbx_scene *scene = ufbx_load_file(filename, NULL, &err);

  if (!scene) {
    fprintf(stderr, "Failed to load FBX: %s\n", err.description.data);
    return false;
  }

  // First pass: count total triangles
  u64 total_triangle_count = 0;
  for (u64 node_idx = 0; node_idx < scene->nodes.count; node_idx++) {
    ufbx_node *node = scene->nodes.data[node_idx];
    if (!node->mesh) continue;

    ufbx_mesh *mesh = node->mesh;
    for (u64 face_idx = 0; face_idx < mesh->faces.count; face_idx++) {
      ufbx_face face = mesh->faces.data[face_idx];
      // Assert all faces are triangles
      assert(face.num_indices == 3);
    }

    total_triangle_count += mesh->faces.count;
  }

  // Allocate triangle array
  Triangle *tris = malloc(total_triangle_count * sizeof(Triangle));
  u64 tri_idx = 0;

  // Second pass: extract and transform triangles
  for (u64 node_idx = 0; node_idx < scene->nodes.count; node_idx++) {
    ufbx_node *node = scene->nodes.data[node_idx];
    if (!node->mesh) continue;

    ufbx_mesh *mesh = node->mesh;

    // Get the node's local transform matrix
    ufbx_matrix node_to_world = node->node_to_world;

    for (u64 face_idx = 0; face_idx < mesh->faces.count; face_idx++) {
      ufbx_face face = mesh->faces.data[face_idx];

      // Extract the three vertex indices for this triangle
      for (u32 vert_idx = 0; vert_idx < 3; vert_idx++) {
        u32 index = mesh->vertex_indices.data[face.index_begin + vert_idx];
        ufbx_vec3 pos = mesh->vertices.data[index];

        // Transform vertex by node's local transform
        ufbx_vec3 transformed = ufbx_transform_position(&node_to_world, pos);

        tris[tri_idx].p[vert_idx] = (vec3s){{
          (f32)transformed.x,
          (f32)transformed.y,
          (f32)transformed.z,
        }};
      }

      tri_idx++;
    }
  }

  *triangle_count = total_triangle_count;
  *triangles = tris;

  ufbx_free_scene(scene);

  return true;
}
