#ifndef VK_H
#define VK_H

#include <volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "toteload.h"

#define KFVK_CHECK(res) do { if ((res) != VK_SUCCESS) PanicMsg("Vulkan call failed.") } while (0)

typedef enum {
  KFVK_USE_VALIDATION = 1 << 0,
} VkInstanceCreateFlag;

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

  u32 storage_size;
  VkBuffer storage_buffer;
  VkDeviceMemory storage_memory;

  VkFence fence;

  VkDebugUtilsMessengerEXT messenger;
} kfvk_State;

b32 kfvk_create(kfvk_State *state, Arena *scratch, u32 flags);

b32 kfvk_dispatch(kfvk_State *state);

#endif // VK_H
