#include "vk.h"
#include "model.h"

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

internal b32 find_memory_type(
  VkPhysicalDevice physical_device,
  u32 type_bits,
  VkMemoryPropertyFlags flags,
  u32 *out)
{
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

b32 kfvk_create_buffer(
  kfvk_Buffer *b,
  VkPhysicalDevice physical_device,
  VkDevice device,
  VkDeviceSize size,
  VkBufferUsageFlags usage,
  VkMemoryPropertyFlags props)
{
  VkBufferCreateInfo buffer_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VkBuffer buffer;
  VK_TRY(vkCreateBuffer(device, &buffer_info, Null, &buffer));

  VkMemoryRequirements reqs;
  vkGetBufferMemoryRequirements(device, buffer, &reqs);

  u32 memory_type_index;
  if (!find_memory_type(
        physical_device,
        reqs.memoryTypeBits,
        props,
        &memory_type_index))
  {
    Todo();
  }

  VkMemoryAllocateFlagsInfo flags_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
    .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
  };

  VkMemoryAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &flags_info : Null,
    .allocationSize = reqs.size,
    .memoryTypeIndex = memory_type_index,
  };

  VkDeviceMemory memory;
  VK_TRY(vkAllocateMemory(device, &alloc_info, Null, &memory));
  VK_TRY(vkBindBufferMemory(device, buffer, memory, 0));

  VkBufferDeviceAddressInfo address_info = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = buffer,
  };

  VkDeviceAddress address = 0;
  if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    address = vkGetBufferDeviceAddress(device, &address_info);
  }

  *b = (kfvk_Buffer){
    .size = size,
    .buffer = buffer,
    .memory = memory,
    .address = address,
  };

  return True;
}

void kfvk_destroy_buffer(kfvk_Buffer *b, VkDevice device) {
  vkDestroyBuffer(device, b->buffer, Null);
  vkFreeMemory(device, b->memory, Null);
  zero_struct(kfvk_Buffer, b);
}

#define MAX_EXTENSIONS 16

#if 0
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
    if (!kfvk_create_buffer(
          &state->storage,
          physical_device,
          device,
          buffer_size,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
      return False;
    }
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
      .buffer = state->storage.buffer,
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
#endif

internal b32 build_as_common(
  kfvk_AccelerationStructure *as,
  kfvk_Graphics *g,
  VkAccelerationStructureTypeKHR type,
  VkAccelerationStructureGeometryKHR const *geometry,
  u32 primitive_count
) {
  VkAccelerationStructureBuildGeometryInfoKHR build_info = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = type,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    .geometryCount = 1,
    .pGeometries = geometry,
  };

  VkAccelerationStructureBuildSizesInfoKHR sizes = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };

  vkGetAccelerationStructureBuildSizesKHR(
    g->device,
    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
    &build_info,
    &primitive_count,
    &sizes);

  kfvk_Buffer buffer;
  kfvk_create_buffer(
    &buffer,
    g->physical_device,
    g->device,
    sizes.accelerationStructureSize,
    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VkAccelerationStructureCreateInfoKHR as_info = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    .buffer = buffer.buffer,
    .offset = 0,
    .size = sizes.accelerationStructureSize,
    .type = type,
  };

  VkAccelerationStructureKHR handle;
  VK_TRY(vkCreateAccelerationStructureKHR(g->device, &as_info, Null, &handle));

  VkPhysicalDeviceAccelerationStructurePropertiesKHR as_props = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
  };

  VkPhysicalDeviceProperties2 props = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
    .pNext = &as_props,
  };

  vkGetPhysicalDeviceProperties2(g->physical_device, &props);

  u32 scratch_align = as_props.minAccelerationStructureScratchOffsetAlignment;

  kfvk_Buffer scratch;
  kfvk_create_buffer(
    &scratch,
    g->physical_device,
    g->device,
    sizes.buildScratchSize + scratch_align,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VkDeviceAddress scratch_address = round_up_to_power_of_two(scratch.address, scratch_align);

  build_info.dstAccelerationStructure = handle;
  build_info.scratchData.deviceAddress = scratch_address;

  VkAccelerationStructureBuildRangeInfoKHR range = {
    .primitiveCount = primitive_count,
    .primitiveOffset = 0,
    .firstVertex = 0,
    .transformOffset = 0,
  };

  VkAccelerationStructureBuildRangeInfoKHR const *ranges[] = { &range };

  VkCommandBuffer cmds = g->command_buffer;

  VK_TRY(vkResetCommandBuffer(cmds, 0));

  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VK_TRY(vkBeginCommandBuffer(cmds, &begin_info));
  vkCmdBuildAccelerationStructuresKHR(cmds, 1, &build_info, ranges);
  VK_TRY(vkEndCommandBuffer(cmds));

  VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmds,
  };

  VK_TRY(vkResetFences(g->device, 1, &g->fence));
  VK_TRY(vkQueueSubmit(g->queue, 1, &submit_info, g->fence));
  VK_TRY(vkWaitForFences(g->device, 1, &g->fence, VK_TRUE, UINT64_MAX));

  kfvk_destroy_buffer(&scratch, g->device);

  VkAccelerationStructureDeviceAddressInfoKHR as_address_info = {
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
    .accelerationStructure = handle,
  };

  VkDeviceAddress address = vkGetAccelerationStructureDeviceAddressKHR(g->device, &as_address_info);

  *as = (kfvk_AccelerationStructure){
    .handle = handle,
    .address = address,
    .buffer = buffer,
  };

  return True;
}

b32 kfvk_create_graphics(kfvk_Graphics *g, KfVkCreateGraphicsFlags flags) {
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

  g->messenger = VK_NULL_HANDLE;
  if (flags & KFVK_USE_VALIDATION) {
    vkCreateDebugUtilsMessengerEXT(inst, &messenger_info, Null, &g->messenger);
  }

  g->instance = inst;

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

  VkPhysicalDevice physical_device = devices[i_dev];

  g->physical_device = physical_device;

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

  g->queue_family_index = queue_family_index;

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

  g->device = device;

  volkLoadDevice(device);

  vkGetDeviceQueue(device, queue_family_index, 0, &g->queue);

  VkCommandPoolCreateInfo pool_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = queue_family_index,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
  };

  VkCommandPool command_pool;
  VK_TRY(vkCreateCommandPool(device, &pool_info, Null, &command_pool));

  g->command_pool = command_pool;

  VkCommandBufferAllocateInfo buffer_alloc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  VK_TRY(vkAllocateCommandBuffers(device, &buffer_alloc_info, &g->command_buffer));

  // Fence
  {
    VkFenceCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    VK_TRY(vkCreateFence(device, &info, Null, &g->fence));
  }

  return True;
}

b32 kfvk_create_raytracing_resources(
  kfvk_RayTracing *rt,
  kfvk_Graphics *g,
  Arena *scratch,
  Triangle const *triangles,
  u64 triangle_count)
{
  VkDescriptorSetLayout descriptor_set_layout;
  {
    VkDescriptorSetLayoutBinding bindings[] = {
      {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
      {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
      {
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
    };

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = Count_of(bindings),
      .pBindings = bindings,
    };

    VK_TRY(vkCreateDescriptorSetLayout(g->device, &descriptor_set_layout_info, Null, &descriptor_set_layout));
  }

  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  {
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set_layout,
    };

    VK_TRY(vkCreatePipelineLayout(g->device, &pipeline_layout_info, Null, &pipeline_layout));

    String base_path = string_from_cstr(SDL_GetBasePath());

    String path = arena_copy_string(scratch, base_path);
    arena_copy_string(scratch, string_lit("trace_test.comp.glsl.spv"));
    arena_append(u8, scratch, '\0');

    SDL_IOStream *f = SDL_IOFromFile(Cast(char const*,path.str), "rb");
    if (!f) {
      Todo();
    }

    u64 shader_size = SDL_GetIOSize(f);
    if (shader_size < 0) {
      Todo();
    }

    shader_size = round_up_to_power_of_two(shader_size, 4);
    void *shader_data = arena_push_array(u8, scratch, shader_size);
    u64 len = SDL_ReadIO(f, shader_data, shader_size);
    if (len != shader_size) {
      Todo();
    }

    VkShaderModuleCreateInfo shader_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = shader_size,
      .pCode = shader_data,
    };

    VkShaderModule shader_module;
    VK_TRY(vkCreateShaderModule(g->device, &shader_info, Null, &shader_module));

    VkComputePipelineCreateInfo compute_pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader_module,
        .pName = "main",
      },
      .layout = pipeline_layout,
    };

    VK_TRY(vkCreateComputePipelines(g->device, Null, 1, &compute_pipeline_info, Null, &pipeline));
    vkDestroyShaderModule(g->device, shader_module, Null);
  }

  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;
  {
    VkDescriptorPoolSize pool_sizes[] = {
      {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
      },
      {
        .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1,
      },
      {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
      }
    };

    VkDescriptorPoolCreateInfo descriptor_pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = Count_of(pool_sizes),
      .pPoolSizes = pool_sizes,
    };

    VK_TRY(vkCreateDescriptorPool(g->device, &descriptor_pool_info, Null, &descriptor_pool));

    VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &descriptor_set_layout,
    };

    VK_TRY(vkAllocateDescriptorSets(g->device, &descriptor_set_alloc_info, &descriptor_set));
  }

  

  kfvk_Buffer color_image_buffer;
  {
    VkDeviceSize buffer_size = 1280 * 960 * 3 * sizeof(f32);
    if (!kfvk_create_buffer(
          &color_image_buffer,
          g->physical_device,
          g->device,
          buffer_size,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
      return False;
    }
  }

  kfvk_Buffer context_buffer;
  {
    if (!kfvk_create_buffer(
          &context_buffer,
          g->physical_device,
          g->device,
          sizeof(RaytraceContext),
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
      return False;
    }
  }

  VkDeviceSize size = triangle_count * sizeof(Triangle);

  kfvk_Buffer vertex_buffer;
  kfvk_AccelerationStructure blas;
  kfvk_Buffer instance_buffer;
  kfvk_AccelerationStructure tlas;

  {
    kfvk_create_buffer(
      &vertex_buffer,
      g->physical_device,
      g->device,
      size,
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
      | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
      | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void *p;
    VK_TRY(vkMapMemory(g->device, vertex_buffer.memory, 0, VK_WHOLE_SIZE, 0, &p));
    memcpy(p, triangles, size);
    vkUnmapMemory(g->device, vertex_buffer.memory);
  }

  {
    VkAccelerationStructureGeometryKHR geometry = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
      .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
      .geometry.triangles = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .indexType = VK_INDEX_TYPE_NONE_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = vertex_buffer.address },
        .vertexStride = 3 * sizeof(f32),
        .maxVertex = 3 * triangle_count - 1,
      },
    };

    b32 ok = build_as_common(
      &blas,
      g,
      VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
      &geometry,
      triangle_count);
    if (!ok) {
      return False;
    }
  }

  {
    VkAccelerationStructureInstanceKHR instance = {
      .transform = {
        .matrix = {
          { 0.1f, 0.0f, 0.0f, 0.0f },
          { 0.0f, 0.1f, 0.0f, 0.0f },
          { 0.0f, 0.0f, 0.1f, 0.0f },
        },
      },
      .instanceCustomIndex = 0,
      .mask = 0xff,
      .instanceShaderBindingTableRecordOffset = 0,
      .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
      .accelerationStructureReference = blas.address,
    };

    kfvk_create_buffer(
      &instance_buffer,
      g->physical_device,
      g->device,
      sizeof(instance),
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
      | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void *p;
    VK_TRY(vkMapMemory(g->device, instance_buffer.memory, 0, VK_WHOLE_SIZE, 0, &p));
    memcpy(p, &instance, sizeof(instance));
    vkUnmapMemory(g->device, instance_buffer.memory);
  }

  {
    VkAccelerationStructureGeometryKHR geometry = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
      .geometry.instances = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data.deviceAddress = instance_buffer.address,
      },
    };

    b32 ok = build_as_common(
      &tlas,
      g,
      VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      &geometry, 1);
    if (!ok) {
      return False;
    }
  }

  kfvk_destroy_buffer(&instance_buffer, g->device);

  {
    VkWriteDescriptorSet writes[] = {
      {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &(VkDescriptorBufferInfo){
          .buffer = color_image_buffer.buffer,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
        },
      },
      {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .pNext = &(VkWriteDescriptorSetAccelerationStructureKHR){
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
          .accelerationStructureCount = 1,
          .pAccelerationStructures = &tlas.handle,
        },
      },
      {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 2,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &(VkDescriptorBufferInfo){
          .buffer = context_buffer.buffer,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
        },
      },
    };

    vkUpdateDescriptorSets(g->device, Count_of(writes), writes, 0, Null);
  }

  *rt = (kfvk_RayTracing){
    .pipeline_layout = pipeline_layout,
    .pipeline = pipeline,
    .descriptor_set_layout = descriptor_set_layout,
    .descriptor_pool = descriptor_pool,
    .descriptor_set = descriptor_set,
    .context_buffer = context_buffer,
    .color_image_buffer = color_image_buffer,
    .vertex_buffer = vertex_buffer,
    .blas = blas,
    .tlas = tlas,
  };

  return True;
}

b32 kfvk_rt_update_context(kfvk_Graphics *g, kfvk_RayTracing *r, CameraBasis const *cam) {
  void *p;
  VK_TRY(vkMapMemory(g->device, r->context_buffer.memory, 0, VK_WHOLE_SIZE, 0, &p));
  RaytraceContext context = {
    .cam_position = cam->position,
    .cam_forward = cam->forward,
    .cam_du = cam->du,
    .cam_dv = cam->dv,
  };
  memcpy(p, &context, sizeof(RaytraceContext));
  vkUnmapMemory(g->device, r->context_buffer.memory);

  return True;
}

b32 kfvk_rt_dispatch(kfvk_Graphics *g, kfvk_RayTracing *r) {
  VkCommandBuffer cmds = g->command_buffer;

  VK_TRY(vkResetCommandBuffer(cmds, 0));

  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VK_TRY(vkBeginCommandBuffer(cmds, &begin_info));

  vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, r->pipeline);
  vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, r->pipeline_layout, 0, 1, &r->descriptor_set, 0, Null);
  vkCmdDispatch(cmds, 1280/16, 960/8, 1);

  VkMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
  };

  vkCmdPipelineBarrier(
    cmds, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
    1, &barrier,
    0, Null,
    0, Null);

  VK_TRY(vkEndCommandBuffer(cmds));

  VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &cmds,
  };

  VK_TRY(vkResetFences(g->device, 1, &g->fence));
  VK_TRY(vkQueueSubmit(g->queue, 1, &submit_info, g->fence));
  VK_TRY(vkWaitForFences(g->device, 1, &g->fence, VK_TRUE, UINT64_MAX));

  return True;

}
