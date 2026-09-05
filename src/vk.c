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

internal u32 find_memory_type(
  VkPhysicalDevice physical_device,
  u32 type_bits,
  VkMemoryPropertyFlags flags)
{
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &props);

  for (u32 i = 0; i < props.memoryTypeCount; i++) {
    b32 is_allowed = (type_bits & (1u << i)) != 0;
    b32 has_props = (props.memoryTypes[i].propertyFlags & flags) == flags;

    if (is_allowed && has_props) {
      return i;
    }
  }

  return UINT32_MAX;
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

  u32 memory_type_index = find_memory_type(
        physical_device,
        reqs.memoryTypeBits,
        props);
  if (memory_type_index == UINT32_MAX) {
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

b32 kfvk_create_image(kfvk_Graphics *g, kfvk_Image *i, kfvk_ImageOptions const *o) {
  VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

  VkImageCreateInfo image_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .extent = { .width = o->width, .height = o->height, .depth = 1 },
    .mipLevels = 1,
    .arrayLayers = 1,
    .format = format,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .samples = VK_SAMPLE_COUNT_1_BIT,
  };

  VkImage image;
  VK_TRY(vkCreateImage(g->device, &image_info, Null, &image));

  VkMemoryRequirements reqs;
  vkGetImageMemoryRequirements(g->device, image, &reqs);

  u32 memory_type_index = find_memory_type(
    g->physical_device,
    reqs.memoryTypeBits,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (memory_type_index == UINT32_MAX) {
    Todo();
  }

  VkMemoryAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = reqs.size,
    .memoryTypeIndex = memory_type_index,
  };

  VkDeviceMemory memory;
  VK_TRY(vkAllocateMemory(g->device, &alloc_info, Null, &memory));
  VK_TRY(vkBindImageMemory(g->device, image, memory, 0));

  VkImageViewCreateInfo view_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = format,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1,
      .layerCount = 1,
    },
  };

  VkImageView view;
  VK_TRY(vkCreateImageView(g->device, &view_info, Null, &view));

  *i = (kfvk_Image){
    .image = image,
    .view = view,
    .memory = memory,
  };

  return True;
}

#define MAX_EXTENSIONS 16

internal b32 build_as_common(
  kfvk_AccelerationStructure *as,
  kfvk_Graphics *g,
  VkCommandBuffer cmds,
  VkFence fence,
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

  VK_TRY(vkResetFences(g->device, 1, &fence));
  VK_TRY(vkQueueSubmit(g->queue, 1, &submit_info, fence));
  VK_TRY(vkWaitForFences(g->device, 1, &fence, VK_TRUE, UINT64_MAX));

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

b32 kfvk_create_graphics(kfvk_Graphics *g, SDL_Window *window, KfVkCreateGraphicsFlags flags) {
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

  VkSurfaceKHR surface;
  if (!SDL_Vulkan_CreateSurface(window, inst, Null, &surface)) {
    SDL_Log("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
    return False;
  }

  g->surface = surface;

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
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, Null);

  VkQueueFamilyProperties queue_families[queue_family_count];
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

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

  if (!SDL_Vulkan_GetPresentationSupport(inst, physical_device, queue_family_index)) {
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

  // Device
  // ---------------------------------------------------------------------------

  VkPhysicalDeviceVulkan12Features vk_1_2_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .bufferDeviceAddress = VK_TRUE,
  };

  VkPhysicalDeviceVulkan13Features vk_1_3_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .pNext = &vk_1_2_features,
    .synchronization2 = VK_TRUE,
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
  VK_TRY(vkCreateDevice(physical_device, &device_info, Null, &device));

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
    .commandBufferCount = Count_of(g->command_buffer),
  };

  VK_TRY(vkAllocateCommandBuffers(device, &buffer_alloc_info, g->command_buffer));

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
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
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

    VK_TRY(vkCreateDescriptorSetLayout(g->device, &descriptor_set_layout_info, Null, &descriptor_set_layout));
  }

  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  {
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set_layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &(VkPushConstantRange){
        .offset = 0,
        .size = sizeof(RaytraceContext),
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      },
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

    VK_TRY(vkCreateComputePipelines(
      g->device, Null, 1, &compute_pipeline_info, Null, &pipeline));
    vkDestroyShaderModule(g->device, shader_module, Null);
  }

  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;
  {
    VkDescriptorPoolSize pool_sizes[] = {
      {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
      },
      {
        .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1,
      },
      {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
      },
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
      g->command_buffer[0],
      g->fence,
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

  kfvk_Image color_image;
  {
    b32 ok = kfvk_create_image(g, &color_image, &(kfvk_ImageOptions){
      .width = 1280,
      .height = 960,
    });
    if (!ok) {
      Todo();
    }

    VkCommandBuffer cmds = g->command_buffer[0];

    VK_TRY(vkResetCommandBuffer(cmds, 0));

    VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VK_TRY(vkBeginCommandBuffer(cmds, &begin_info));

    VkImageMemoryBarrier ready_color_image_barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_NONE,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = color_image.image,
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
      },
    };

    vkCmdPipelineBarrier(
      cmds,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
      0, Null,
      0, Null,
      1, &ready_color_image_barrier);

    VK_TRY(vkEndCommandBuffer(cmds));

    VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmds,
    };

    VK_TRY(vkResetFences(g->device, 1, &g->fence));
    VK_TRY(vkQueueSubmit(g->queue, 1, &submit_info, g->fence));
    VK_TRY(vkWaitForFences(g->device, 1, &g->fence, VK_TRUE, UINT64_MAX));
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
      g->command_buffer[0],
      g->fence,
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
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &(VkDescriptorImageInfo){
          .sampler = VK_NULL_HANDLE,
          .imageView = color_image.view,
          .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
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
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &(VkDescriptorBufferInfo){
          .buffer = vertex_buffer.buffer,
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
    .color_image = color_image,
    .vertex_buffer = vertex_buffer,
    .blas = blas,
    .tlas = tlas,
  };

  return True;
}

b32 kfvk_rt_dispatch(
  kfvk_Graphics *g,
  kfvk_RayTracing *r,
  kfvk_Swapchain *s,
  RaytraceContext const *context)
{
  VkCommandBuffer cmds = g->command_buffer[s->frame_idx];

  VK_TRY(vkResetCommandBuffer(cmds, 0));

  VkCommandBufferBeginInfo begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VK_TRY(vkBeginCommandBuffer(cmds, &begin_info));

  VkImageMemoryBarrier ready_color_image_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = r->color_image.image,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1,
      .layerCount = 1,
    },
  };

  vkCmdPipelineBarrier(
    cmds,
    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
    0, Null,
    0, Null,
    1, &ready_color_image_barrier);

  vkCmdPushConstants(
    cmds,
    r->pipeline_layout,
    VK_SHADER_STAGE_COMPUTE_BIT,
    0, sizeof(RaytraceContext), context);

  vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_COMPUTE, r->pipeline);
  vkCmdBindDescriptorSets(
    cmds,
    VK_PIPELINE_BIND_POINT_COMPUTE,
    r->pipeline_layout,
    0,
    1, &r->descriptor_set,
    0, Null);
  vkCmdDispatch(cmds, 1280/16, 960/8, 1);

  VkImageMemoryBarrier present_src_color_image_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = r->color_image.image,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1,
      .layerCount = 1,
    },
  };

  VkImageMemoryBarrier present_ready_swapchain_image_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = s->images[s->image_idx],
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1,
      .layerCount = 1,
    },
  };

  VkImageMemoryBarrier present_image_barriers[] = {
    present_src_color_image_barrier,
    present_ready_swapchain_image_barrier,
  };

  vkCmdPipelineBarrier(
    cmds,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
    0, Null,
    0, Null,
    Count_of(present_image_barriers), present_image_barriers);

  vkCmdBlitImage(
    cmds,
    r->color_image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    s->images[s->image_idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1, &(VkImageBlit){
      .srcSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .layerCount = 1,
      },
      .dstSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .layerCount = 1,
      },
      .srcOffsets = { {0,0,0}, {1280,960,1}, },
      .dstOffsets = { {0,960,0}, {1280,0,1}, },
    },
    VK_FILTER_LINEAR);

  VkImageMemoryBarrier present_swapchain_image_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .dstAccessMask = 0,
    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = s->images[s->image_idx],
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = 1,
      .layerCount = 1,
    },
  };

  vkCmdPipelineBarrier(
    cmds,
    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
    0, Null,
    0, Null,
    1, &present_swapchain_image_barrier);

  VK_TRY(vkEndCommandBuffer(cmds));

  VkSemaphoreSubmitInfo sema_acquire_submit_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = s->sema_acquire[s->frame_idx],
    .stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
  };

  VkCommandBufferSubmitInfo commands_submit_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .commandBuffer = cmds,
  };

  VkSemaphoreSubmitInfo sema_render_finished_submit_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = s->sema_render_finished[s->image_idx],
    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
  };

  VkSubmitInfo2 submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .waitSemaphoreInfoCount = 1,
    .pWaitSemaphoreInfos = &sema_acquire_submit_info,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &commands_submit_info,
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos = &sema_render_finished_submit_info,
  };

  VK_TRY(vkQueueSubmit2(g->queue, 1, &submit_info, s->fences[s->frame_idx]));

  return True;
}

b32 kfvk_create_swapchain(kfvk_Swapchain *s, kfvk_Graphics *g, SDL_Window *window) {
  VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;
  VkColorSpaceKHR colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
  VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  VkSurfaceCapabilitiesKHR surface_capabilities;
  VK_TRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g->physical_device, g->surface, &surface_capabilities));
  if ((surface_capabilities.supportedUsageFlags & usage) != usage) {
    SDL_Log("Surface does not support necessary flags");
    return False;
  }

  VkExtent2D extent = surface_capabilities.currentExtent;
  if (extent.width == UINT32_MAX) {
    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);

    extent.width = Clamp(
      surface_capabilities.minImageExtent.width,
      surface_capabilities.maxImageExtent.width,
      Cast(u32,width));

    extent.height = Clamp(
      surface_capabilities.minImageExtent.height,
      surface_capabilities.maxImageExtent.height,
      Cast(u32,height));
  }

  if (extent.width == 0 || extent.height == 0) {
    return False;
  }

  u32 image_count = surface_capabilities.minImageCount + 1;

  if (image_count > KFVK_MAX_SWAPCHAIN_IMAGES) {
    Todo();
  }

  VkSwapchainKHR old_swapchain = s->handle;

  VkSwapchainCreateInfoKHR swapchain_info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = g->surface,
    .minImageCount = image_count,
    .imageFormat = format,
    .imageColorSpace = colorSpace,
    .imageExtent = extent,
    .imageArrayLayers = 1,
    .imageUsage = usage,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .preTransform = surface_capabilities.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = present_mode,
    .clipped = VK_TRUE,
    .oldSwapchain = old_swapchain,
  };

  VkSwapchainKHR swapchain;
  VK_TRY(vkCreateSwapchainKHR(g->device, &swapchain_info, Null, &swapchain));

  if (old_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(g->device, old_swapchain, Null);
  }

  u32 new_image_count = 0;
  VK_TRY(vkGetSwapchainImagesKHR(g->device, swapchain, &new_image_count, Null));

  if (new_image_count > KFVK_MAX_SWAPCHAIN_IMAGES) {
    Todo();
  }

  VK_TRY(vkGetSwapchainImagesKHR(g->device, swapchain, &new_image_count, s->images));

  VkSemaphoreCreateInfo sema_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fence_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  for (u32 i = 0; i < Count_of(s->sema_acquire); i++) {
    VK_TRY(vkCreateFence(g->device, &fence_info, Null, &s->fences[i]));
    VK_TRY(vkCreateSemaphore(g->device, &sema_info, Null, &s->sema_acquire[i]));
  }

  for (u32 i = 0; i < Count_of(s->sema_render_finished); i++) {
    VK_TRY(vkCreateSemaphore(g->device, &sema_info, Null, &s->sema_render_finished[i]));
  }

  s->handle = swapchain;
  s->format = format;
  s->extent = extent;
  s->present_mode = present_mode;
  s->image_count = new_image_count;

  return True;
}

b32 kfvk_swapchain_acquire(kfvk_Graphics *g, kfvk_Swapchain *s) {
  VK_TRY(vkResetFences(g->device, 1, &s->fences[s->frame_idx]));
  VkResult err = vkAcquireNextImageKHR(
    g->device,
    s->handle,
    UINT64_MAX,
    s->sema_acquire[s->frame_idx],
    VK_NULL_HANDLE,
    &s->image_idx);

  if (err) {
    Todo();
  }

  return True;
}

b32 kfvk_swapchain_present_and_wait(kfvk_Graphics *g, kfvk_Swapchain *s) {
  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &s->sema_render_finished[s->image_idx],
    .swapchainCount = 1,
    .pSwapchains = &s->handle,
    .pImageIndices = &s->image_idx,
  };

  VkResult err = vkQueuePresentKHR(g->queue, &present_info);
  if (err && err != VK_SUBOPTIMAL_KHR) {
    Todo();
  }

  s->frame_idx = (s->frame_idx + 1) % KFVK_MAX_IN_FLIGHT_FRAMES;

  VK_TRY(vkWaitForFences(g->device, 1, &s->fences[s->frame_idx], True, UINT64_MAX));

  return True;
}
