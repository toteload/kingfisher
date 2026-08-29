#ifndef VK_H
#define VK_H

#include <volk.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "toteload.h"

typedef enum {
  KFVK_USE_VALIDATION = 1 << 0,
} VkInstanceCreateFlag;

b32 kfvk_instance_create(u32 flags, VkInstance *instance, VkDebugUtilsMessengerEXT *messenger);

#endif // VK_H
