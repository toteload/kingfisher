#include "vk.h"
#include <vulkan/vk_enum_string_helper.h>

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

internal b32 find_memory_type(VkPhysicalDevice physical_device, u32 type_bits, VkMemoryPropertyFlags flags, u32 *out) {
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &props);

  for (u32 i = 0; i < props.memoryTypeCount; i++) {
    b32 is_allowed = (type_bits & (1u << i)) != 0;
    b32 has_props = (props.memoryTypes[i].propertyFlags & flags) == flags;

    if (is_allowed && has_props) {
      *out = i;
      return True;
    }
  }

  return False;
}

#define MAX_EXTENSIONS 16

#define VK_TRY(call) do { VkResult tfvk__res = (call); if (tfvk__res != VK_SUCCESS) { SDL_Log("Call %s failed with %s in %s at %s:%d", #call, string_VkResult(tfvk__res), TTLD_FUNC, __FILE__, __LINE__); return False; } } while (0)

b32 kfvk_create(kfvk_State *state, Arena *scratch, u32 flags) {
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
  VK_TRY(vkCreateInstance(&instance_info, Null, &inst));

  volkLoadInstanceOnly(inst);

  state->messenger = VK_NULL_HANDLE;
  if (flags & KFVK_USE_VALIDATION) {
    vkCreateDebugUtilsMessengerEXT(inst, &messenger_info, Null, &state->messenger);
  }

  state->instance = inst;

  // Physical device
  // ---------------------------------------------------------------------------

  u32 device_count = 0;
  VK_TRY(vkEnumeratePhysicalDevices(inst, &device_count, Null));

  VkPhysicalDevice devices[device_count];
  VK_TRY(vkEnumeratePhysicalDevices(inst, &device_count, devices));

  VkPhysicalDeviceProperties2 dev_properties = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
  };

  u32 i_dev = 0;

  vkGetPhysicalDeviceProperties2(devices[i_dev], &dev_properties);

  state->physical_device = devices[i_dev];

  VkPhysicalDevice physical_device = devices[i_dev];

  SDL_Log("Device name: %s", dev_properties.properties.deviceName);

  // Queue
  // ---------------------------------------------------------------------------

  u32 queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(devices[i_dev], &queue_family_count, Null);

  VkQueueFamilyProperties queue_families[queue_family_count];
  vkGetPhysicalDeviceQueueFamilyProperties(devices[i_dev], &queue_family_count, queue_families);

  u32 queue_family_index = UINT32_MAX;
  for (u32 i = 0; i < queue_family_count; i++) {
    if (queue_families[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
      queue_family_index = i;
      break;
    }
  }

  if (queue_family_index == UINT32_MAX) {
    Panic();
  }

  if (!SDL_Vulkan_GetPresentationSupport(inst, devices[i_dev], queue_family_index)) {
    SDL_Log("%s", SDL_GetError());
    return False;
  }

  f32 queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = queue_family_index,
    .queueCount = 1,
    .pQueuePriorities = &queue_priority,
  };

  state->queue_family_index = queue_family_index;

  VkPhysicalDeviceVulkan12Features vk_1_2_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .bufferDeviceAddress = VK_TRUE,
  };

  VkPhysicalDeviceVulkan13Features vk_1_3_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .pNext = &vk_1_2_features,
  };

  VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    .pNext = &vk_1_3_features,
    .accelerationStructure = VK_TRUE,
  };

  VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    .pNext = &acceleration_structure_features,
    .rayQuery = True,
  };

  char const *device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
  };

  VkDeviceCreateInfo device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = &ray_query_features,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &queue_info,
    .enabledExtensionCount = Count_of(device_extensions),
    .ppEnabledExtensionNames = device_extensions,
    .pEnabledFeatures = Null,
  };

  VkDevice device;
  VK_TRY(vkCreateDevice(devices[i_dev], &device_info, Null, &device));

  state->device = device;

  volkLoadDevice(device);

  vkGetDeviceQueue(device, queue_family_index, 0, &state->queue);

  VkCommandPoolCreateInfo pool_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = queue_family_index,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
  };

  VkCommandPool command_pool;
  VK_TRY(vkCreateCommandPool(device, &pool_info, Null, &command_pool));

  state->command_pool = command_pool;

  VkCommandBufferAllocateInfo buffer_alloc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  VK_TRY(vkAllocateCommandBuffers(device, &buffer_alloc_info, &state->command_buffer));

  String base_path = string_from_cstr(SDL_GetBasePath());

  String path = arena_copy_string(scratch, base_path);
  arena_copy_string(scratch, string_lit("test.comp.glsl.spv"));
  arena_append(u8, scratch, '\0');

  SDL_IOStream *f = SDL_IOFromFile(path.str, "rb");
  if (!f) {
    Todo();
  }

  i64 shader_size = SDL_GetIOSize(f);
  if (shader_size < 0) {
    Todo();
  }

  shader_size = round_up_to_power_of_two(shader_size, 4);
  void *shader_data = arena_push_array(u8, scratch, shader_size);
  i64 len = SDL_ReadIO(f, shader_data, shader_size);
  if (len != shader_size) {
    Todo();
  }

  VkShaderModuleCreateInfo shader_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = shader_size,
    .pCode = shader_data,
  };

  VkShaderModule shader_module;
  VK_TRY(vkCreateShaderModule(device, &shader_info, Null, &shader_module));

  VkDescriptorSetLayoutBinding bindings[] = {
    {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    },
  };

  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = Count_of(bindings),
    .pBindings = bindings,
  };

  VkDescriptorSetLayout descriptor_set_layout;
  VK_TRY(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_info, Null, &descriptor_set_layout));

  state->descriptor_set_layout = descriptor_set_layout;

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = 1,
    .pSetLayouts = &descriptor_set_layout,
  };

  VK_TRY(vkCreatePipelineLayout(device, &pipeline_layout_info, Null, &state->pipeline_layout));

  VkComputePipelineCreateInfo compute_pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
    .stage = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = shader_module,
      .pName = "main",
    },
    .layout = state->pipeline_layout,
  };

  VK_TRY(vkCreateComputePipelines(device, Null, 1, &compute_pipeline_info, Null, &state->pipeline));

  vkDestroyShaderModule(device, shader_module, Null);

  // Buffer

  {
    VkDeviceSize buffer_size = 1280 * 960 * 3 * sizeof(f32);

    VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = buffer_size,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkBuffer buffer;
    VK_TRY(vkCreateBuffer(device, &buffer_info, Null, &buffer));

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device, buffer, &reqs);

    u32 memory_type_index;
    if (!find_memory_type(physical_device, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory_type_index)) {
      Todo();
    }

    VkMemoryAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = reqs.size,
      .memoryTypeIndex = memory_type_index,
    };

    VkDeviceMemory device_memory;
    VK_TRY(vkAllocateMemory(device, &alloc_info, Null, &device_memory));
    VK_TRY(vkBindBufferMemory(device, buffer, device_memory, 0));

    state->storage_size = buffer_size;
    state->storage_buffer = buffer;
    state->storage_memory = device_memory;
  }

  // Descriptor sets

  {
    VkDescriptorPoolSize pool_sizes[] = {
      {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
      }
    };

    VkDescriptorPoolCreateInfo descriptor_pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = pool_sizes,
    };

    VkDescriptorPool descriptor_pool;
    VK_TRY(vkCreateDescriptorPool(device, &descriptor_pool_info, Null, &descriptor_pool));

    state->descriptor_pool = descriptor_pool;

    VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &state->descriptor_set_layout,
    };

    VkDescriptorSet descriptor_set;
    VK_TRY(vkAllocateDescriptorSets(device, &descriptor_set_alloc_info, &descriptor_set));

    state->descriptor_set = descriptor_set;

    VkDescriptorBufferInfo buffer_info = {
      .buffer = state->storage_buffer,
      .offset = 0,
      .range = VK_WHOLE_SIZE,
    };

    VkWriteDescriptorSet writes[] = {
      {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer_info,
      },
    };

    vkUpdateDescriptorSets(device, 1, writes, 0, Null);
  }

  // Fence
  {
    VkFenceCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    VK_TRY(vkCreateFence(device, &info, Null, &state->fence));
  }

  return True;
}

b32 kfvk_dispatch(kfvk_State *state) {
  VkCommandBuffer cmds = state->command_buffer;

  VK_TRY(vkResetCommandBuffer(cmds, 0));

  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VK_TRY(vkBeginCommandBuffer(cmds, &begin_info));

  vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, state->pipeline);
  vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, state->pipeline_layout, 0, 1, &state->descriptor_set, 0, Null);
  vkCmdDispatch(cmds, 1280/16, 960/8, 1);

  VkMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
  };

  vkCmdPipelineBarrier(
      cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
       0,
       1, &barrier,
       0, Null,
       0, Null);

  VK_TRY(vkEndCommandBuffer(cmds));

  VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmds,
  };

  VK_TRY(vkResetFences(state->device, 1, &state->fence));
  VK_TRY(vkQueueSubmit(state->queue, 1, &submit_info, state->fence));
  VK_TRY(vkWaitForFences(state->device, 1, &state->fence, VK_TRUE, UINT64_MAX));

  return True;
}
