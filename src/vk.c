#include "vk.h"

internal VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  VkDebugUtilsMessageTypeFlagsEXT types,
  VkDebugUtilsMessengerCallbackDataEXT const *data,
  void *user
) {
  Unused(types, user);

  SDL_LogPriority priority = SDL_LOG_PRIORITY_INFO;
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    priority = SDL_LOG_PRIORITY_ERROR;
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    priority = SDL_LOG_PRIORITY_WARN;
  }

  SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "[vk] %s", data->pMessage);

  return VK_FALSE;
}

#define MAX_EXTENSIONS 16

b32 kfvk_instance_create(u32 flags, VkInstance *instance, VkDebugUtilsMessengerEXT *messenger) {
  if (!SDL_Vulkan_LoadLibrary(Null)) {
    SDL_Log("SDL_Vulkan_LoadLibrary failed: %s", SDL_GetError());
    return False;
  }

  volkInitializeCustom(Cast(PFN_vkGetInstanceProcAddr, SDL_Vulkan_GetVkGetInstanceProcAddr()));

  u32 count;
  char const *const *inst_extensions = SDL_Vulkan_GetInstanceExtensions(&count);
  if (is_null(inst_extensions)) {
    SDL_Log("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
    return False;
  }

  char const *extensions[MAX_EXTENSIONS];
  if (count >= MAX_EXTENSIONS) {
    SDL_Log("SDL_Vulkan_GetInstanceExtensions returned more extensions than is supported with MAX_EXTENSIONS");
    return False;
  }

  for (u32 i = 0; i < count; i++) {
    extensions[i] = inst_extensions[i];
  }

  u32 extension_count = count;

  if (flags & KFVK_USE_VALIDATION) {
    extensions[count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    extension_count += 1;
  }

  // ...

  VkDebugUtilsMessengerCreateInfoEXT messenger_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = debug_callback,
  };

  VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "Kingfisher",
    .applicationVersion = VK_MAKE_VERSION(0,1,0),
    .apiVersion = VK_API_VERSION_1_3,
  };

  char const *layers[] = { "VK_LAYER_KHRONOS_validation" };

  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = (flags & KFVK_USE_VALIDATION) ? &messenger_info : Null,
    .pApplicationInfo = &app_info,
    .enabledLayerCount = (flags & KFVK_USE_VALIDATION) ? 1 : 0,
    .ppEnabledLayerNames = layers,
    .enabledExtensionCount = extension_count,
    .ppEnabledExtensionNames = extensions,
  };

  VkInstance inst;
  VkResult res = vkCreateInstance(&instance_info, Null, &inst);
  if (res != VK_SUCCESS) {
    SDL_Log("vkCreateInstance failed: %d", res);
    return False;
  }

  volkLoadInstanceOnly(inst);

  *messenger = VK_NULL_HANDLE;
  if (flags & KFVK_USE_VALIDATION) {
    vkCreateDebugUtilsMessengerEXT(inst, &messenger_info, Null, messenger);
  }

  *instance = inst;

  return True;
}


