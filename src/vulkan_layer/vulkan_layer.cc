#include "vulkan_layer.h"

#define VMA_IMPLEMENTATION
#include <fstream>
#include <iostream>

#include "vk_mem_alloc.h"

vk::ImageCreateInfo VulkanLayer::image2d_create_info(
    vk::Format format, vk::ImageUsageFlags usageFlags, vk::Extent3D extent) {
  vk::ImageCreateInfo info;

  info.imageType = vk::ImageType::e2D;

  info.format = format;
  info.extent = extent;

  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.samples = vk::SampleCountFlagBits::e1;
  info.tiling = vk::ImageTiling::eOptimal;
  info.usage = usageFlags;

  return info;
}

ImageView VulkanLayer::create_2d_image_view(vk::Extent2D extend,
                                            vk::Format format,
                                            vk::ImageUsageFlags usage,
                                            vk::ImageAspectFlags aspect,
                                            VmaMemoryUsage mem_usage) {
  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  VkImage image;
  VmaAllocation image_alloc_info;
  VkExtent3D image_extend = {extend.width, extend.height, 1};
  VkImageCreateInfo ici_c = image2d_create_info(format, usage, image_extend);
  vmaCreateImage(allocator, &ici_c, &alloc_info, &image, &image_alloc_info,
                 nullptr);

  auto vk_image = vk::Image(image);
  auto ivci = image_view2d_create_info(vk_image, format, aspect);

  auto image_view = device.createImageView(ivci);

  return ImageView(ivci.image, image_view);
}

vk::ImageViewCreateInfo VulkanLayer::image_view2d_create_info(
    vk::Image& image, vk::Format format, vk::ImageAspectFlags aspect) {
  vk::ImageViewCreateInfo ivi;
  ivi.setFormat(format);
  vk::ImageSubresourceRange sub_range;
  sub_range.aspectMask = aspect;
  sub_range.baseArrayLayer = 0;
  sub_range.baseMipLevel = 0;
  sub_range.layerCount = 1;
  sub_range.levelCount = 1;
  ivi.setSubresourceRange(sub_range);
  ivi.viewType = vk::ImageViewType::e2D;
  ivi.image = image;
  return ivi;
}

vk::CommandBuffer VulkanLayer::create_command_buffer(
    vk::CommandPool& cmd_pool) {
  vk::CommandBufferAllocateInfo alloc_info;
  alloc_info.commandBufferCount = 1;
  alloc_info.commandPool = cmd_pool;
  alloc_info.level = vk::CommandBufferLevel::ePrimary;
  return device.allocateCommandBuffers(alloc_info)[0];
}

std::vector<char> VulkanLayer::readFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

vk::ShaderModule VulkanLayer::read_shader(const std::string& filename) {
  vk::ShaderModuleCreateInfo ci;
  auto shader_code = readFile(filename);
  ci.codeSize = shader_code.size();
  ci.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());
  return device.createShaderModule(ci);
}

vk::PipelineShaderStageCreateInfo VulkanLayer::create_shader_stage(
    const std::string& filename, const vk::ShaderStageFlagBits stage) {
  vk::PipelineShaderStageCreateInfo shader_info;
  shader_info.stage = stage;
  shader_info.module = read_shader(filename);
  shader_info.pName = "main";
  return shader_info;
}

bool is_device_suitable(const vk::PhysicalDevice& device) {
  auto deviceProperties = device.getProperties();
  return deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
}

uint32_t get_queue_familiy_index(vk::PhysicalDevice& physical_device,
                                 vk::QueueFlags flags) {
  int i = 0;
  for (const auto& queueFamily : physical_device.getQueueFamilyProperties()) {
    if (queueFamily.queueFlags & flags) {
      return i;
    }

    i++;
  }
  return -1;
}

void VulkanLayer::setup_device() {
  for (const auto& device : instance.enumeratePhysicalDevices()) {
    if (is_device_suitable(device)) {
      physical_device = device;
      break;
    }
  }

  if (!physical_device) {
    throw std::runtime_error("failed to find a suitable GPU!");
  }

  vk::DeviceCreateInfo dci;
  std::vector<vk::DeviceQueueCreateInfo> queue_infos(1);

  float queue_priority = 1.0f;
  queue_infos[0].queueCount = 1;
  queue_infos[0].pQueuePriorities = &queue_priority;
  queue_infos[0].queueFamilyIndex =
      get_queue_familiy_index(physical_device, vk::QueueFlagBits::eGraphics);
  dci.setQueueCreateInfos(queue_infos);

  // Enable dynamic rendering
  vk::PhysicalDeviceVulkan13Features features13;
  features13.dynamicRendering = true;
  dci.setPNext(&features13);

  const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  dci.enabledExtensionCount = 1;
  dci.ppEnabledExtensionNames = device_extensions;

  device = physical_device.createDevice(dci);
}

void VulkanLayer::setup_queues() {
  graphics_queue_family =
      get_queue_familiy_index(physical_device, vk::QueueFlagBits::eGraphics);
  graphics_queue = device.getQueue(graphics_queue_family, 0);
}

void VulkanLayer::setup_cmd_pools() {
  graphics_command_pool = create_command_pool(graphics_queue_family);
}

bool VulkanLayer::init_vulkan() {
  vkb::InstanceBuilder builder;
  auto inst_ret = builder.set_app_name("Personal Vulkan Project")
                      .request_validation_layers()
                      .use_default_debug_messenger()
                      .require_api_version(1, 3, 0)
                      .build();
  if (!inst_ret) {
    std::cerr << "Failed to create Vulkan instance. Error: "
              << inst_ret.error().message() << "\n";
    return false;
  }
  vkb_instance = inst_ret.value();

  instance = vk::Instance(vkb_instance.instance);

  setup_device();

  setup_queues();

  setup_cmd_pools();

  init_memory_allocator();

  return true;
}

vk::CommandPool VulkanLayer::create_command_pool(
    uint32_t family, vk::CommandPoolCreateFlags flags) {
  vk::CommandPoolCreateInfo cpi;
  cpi.queueFamilyIndex = family;
  cpi.flags = flags;
  return device.createCommandPool(cpi);
}

VulkanLayer::~VulkanLayer() {
  vkb::destroy_debug_utils_messenger(vkb_instance.instance,
                                     vkb_instance.debug_messenger);
}

bool VulkanLayer::init_memory_allocator() {
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = physical_device;
  allocatorInfo.device = device;
  allocatorInfo.instance = instance;
  vmaCreateAllocator(&allocatorInfo, &allocator);
  return true;
}

void VK_CHECK(vk::Result x) {
  do {
    if ((VkResult)x) {
      std::cout << "Detected Vulkan error: " << x << std::endl;
      abort();
    }
  } while (0);
}
