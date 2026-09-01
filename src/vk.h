#ifndef VK_H
#define VK_H

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "toteload.h"
#include "model.h"
#include "camera.h"

#define VK_CHECK(call) do { VkResult tfvk__res = (call); if (tfvk__res != VK_SUCCESS) { SDL_Log("Call %s failed with %s in %s at %s:%d", #call, string_VkResult(tfvk__res), TTLD_FUNC, __FILE__, __LINE__); } } while (0)

#define VK_TRY(call) do { VkResult tfvk__res = (call); if (tfvk__res != VK_SUCCESS) { SDL_Log("Call %s failed with %s in %s at %s:%d", #call, string_VkResult(tfvk__res), TTLD_FUNC, __FILE__, __LINE__); return False; } } while (0)

typedef enum {
  KFVK_USE_VALIDATION = 1 << 0,
} KfVkCreateGraphicsFlagBit;

typedef u32 KfVkCreateGraphicsFlags;

typedef struct {
  u32 size;
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDeviceAddress address;
} kfvk_Buffer;

typedef struct {
  VkAccelerationStructureKHR handle;
  VkDeviceAddress address;
  kfvk_Buffer buffer;
} kfvk_AccelerationStructure;

typedef struct {
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  u32 queue_family_index;
  VkQueue queue;

  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;

  VkFence fence;

  VkDebugUtilsMessengerEXT messenger;
} kfvk_Graphics;

typedef struct {
  alignas(16) vec3s cam_position;
  alignas(16) vec3s cam_forward;
  alignas(16) vec3s cam_du;
  alignas(16) vec3s cam_dv;
} RaytraceContext;

typedef struct {
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;

  kfvk_Buffer context_buffer;
  kfvk_Buffer color_image_buffer;
  kfvk_Buffer vertex_buffer;
  kfvk_AccelerationStructure blas;
  kfvk_AccelerationStructure tlas;
} kfvk_RayTracing;

b32 kfvk_create_graphics(kfvk_Graphics *graphics, KfVkCreateGraphicsFlags flags);
void kfvk_destroy_graphics(kfvk_Graphics *graphics);

b32 kfvk_create_raytracing_resources(kfvk_RayTracing *rt, kfvk_Graphics *gfx, Arena *scratch, Triangle const *triangles, u64 triangle_count);
void kfvk_destroy_raytracing_resources(kfvk_RayTracing *rt, kfvk_Graphics *gfx);

b32 kfvk_rt_update_context(kfvk_Graphics *gfx, kfvk_RayTracing *rt, CameraBasis const *camera);
b32 kfvk_rt_dispatch(kfvk_Graphics *gfx, kfvk_RayTracing *rt);

b32 kfvk_create_buffer(
  kfvk_Buffer *b,
  VkPhysicalDevice physical_device,
  VkDevice device,
  VkDeviceSize size,
  VkBufferUsageFlags usage,
  VkMemoryPropertyFlags props);
void kfvk_destroy_buffer(kfvk_Buffer *b, VkDevice device);

#endif // VK_H
