#ifndef VK_H
#define VK_H

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "toteload.h"
#include "model.h"

#define VK_CHECK(call) do { VkResult tfvk__res = (call); if (tfvk__res != VK_SUCCESS) { SDL_Log("Call %s failed with %s in %s at %s:%d", #call, string_VkResult(tfvk__res), TTLD_FUNC, __FILE__, __LINE__); } } while (0)

#define VK_TRY(call) do { VkResult tfvk__res = (call); if (tfvk__res != VK_SUCCESS) { SDL_Log("Call %s failed with %s in %s at %s:%d", #call, string_VkResult(tfvk__res), TTLD_FUNC, __FILE__, __LINE__); return False; } } while (0)

typedef enum {
  KFVK_USE_VALIDATION = 1 << 0,
} VkInstanceCreateFlag;

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

  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;

  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;

  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;

  kfvk_Buffer storage;

  VkFence fence;

  kfvk_Buffer vertex_buffer;
  kfvk_AccelerationStructure blas;
  kfvk_AccelerationStructure tlas;

  VkDebugUtilsMessengerEXT messenger;
} kfvk_State;

b32 kfvk_create(kfvk_State *state, Arena *scratch, u32 flags);

b32 build_acceleration_structures(kfvk_State *state, Triangle const *triangles, u64 triangle_count);

b32 kfvk_dispatch(kfvk_State *state);

b32 kfvk_create_buffer(
  kfvk_Buffer *b,
  VkPhysicalDevice physical_device,
  VkDevice device,
  VkDeviceSize size,
  VkBufferUsageFlags usage,
  VkMemoryPropertyFlags props);
void kfvk_destroy_buffer(kfvk_Buffer *b, VkDevice device);

#endif // VK_H
