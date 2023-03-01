#include <SDL.h>
#include <SDL_vulkan.h>

#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <memory>
#include <vulkan/vulkan.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "VkBootstrap.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

void VK_CHECK(vk::Result x) {
  do {
    if ((VkResult)x) {
      std::cout << "Detected Vulkan error: " << x << std::endl;
      abort();
    }
  } while (0);
}

struct ProjectionData {
  glm::mat4 render_mat;
  glm::mat4 normal_mat;
};

struct SyncStructres {
  vk::Fence render_fence;
  vk::Semaphore aquire_sem;
  vk::Semaphore render_sem;
};

struct MeshPushConstants {
  glm::mat4 render_matrix;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 tex_coord;
};

class Mesh {
 public:
  vk::Buffer vertecies;
  vk::Buffer indices;

  uint32_t num_indices;

  static Mesh load(const std::string& filepath, VmaAllocator& allocator) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                          filepath.c_str())) {
      throw std::runtime_error(warn + err);
    }

    std::vector<Vertex> vertecies;
    std::vector<uint32_t> indices;

    for (const auto& shape : shapes) {
      for (const auto& index : shape.mesh.indices) {
        Vertex vertex{};

        vertex.position = {attrib.vertices[3 * index.vertex_index + 0],
                           attrib.vertices[3 * index.vertex_index + 1],
                           attrib.vertices[3 * index.vertex_index + 2]};

        vertex.normal = {attrib.normals[3 * index.normal_index + 0],
                         attrib.normals[3 * index.normal_index + 1],
                         attrib.normals[3 * index.normal_index + 2]};

        vertex.tex_coord = {
            attrib.texcoords[2 * index.texcoord_index + 0],
            1.f - attrib.texcoords[2 * index.texcoord_index + 1]};

        vertecies.push_back(vertex);
        indices.push_back(indices.size());
      }
    }

    Mesh output;

    output.num_indices = indices.size();

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = vertecies.size() * sizeof(Vertex);
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VkBuffer tmp_vert_buffer;
    VmaAllocation tmp_vert_buffer_alloc_info;
    VK_CHECK(vk::Result(vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo,
                                        &tmp_vert_buffer,
                                        &tmp_vert_buffer_alloc_info, nullptr)));
    void* data;
    vmaMapMemory(allocator, tmp_vert_buffer_alloc_info, &data);

    memcpy(data, vertecies.data(), vertecies.size() * sizeof(Vertex));

    vmaUnmapMemory(allocator, tmp_vert_buffer_alloc_info);
    output.vertecies = vk::Buffer(tmp_vert_buffer);

    bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    VkBuffer tmp_index_buffer;
    VmaAllocation tmp_index_buffer_alloc_info;
    VK_CHECK(vk::Result(vmaCreateBuffer(
        allocator, &bufferInfo, &vmaallocInfo, &tmp_index_buffer,
        &tmp_index_buffer_alloc_info, nullptr)));
    vmaMapMemory(allocator, tmp_index_buffer_alloc_info, &data);

    memcpy(data, indices.data(), indices.size() * sizeof(uint32_t));

    vmaUnmapMemory(allocator, tmp_index_buffer_alloc_info);
    output.indices = vk::Buffer(tmp_index_buffer);

    return output;
  }
};

struct SDLWindowDestroyer {
  void operator()(SDL_Window* w) const { SDL_DestroyWindow(w); }
};

class TMP {
 public:
  vk::Extent2D window_extent{1700, 800};

  vkb::Instance vkb_instance;

  vk::Instance instance;

  std::unique_ptr<SDL_Window, SDLWindowDestroyer> sdl_window;
  VkSurfaceKHR surface;

  vk::PhysicalDevice physical_device;
  vk::Device device;

  uint32_t graphics_queue_family;
  vk::Queue graphics_queue;
  vk::CommandPool graphics_command_pool;

  vk::SwapchainKHR swapchain;
  vk::Extent2D swapchain_extend;
  vk::Format swapchain_format;
  std::vector<vk::Image> swapchain_images;
  std::vector<vk::ImageView> swapchain_image_views;
  vk::ImageView depth_image_view;

  vk::Pipeline pipeline;
  vk::PipelineLayout pipeline_layout;

  vk::DescriptorSetLayout global_descriptor_layout;
  vk::DescriptorSet global_desc_set;

  VmaAllocator allocator;

  void* proj_data_ptr;
  vk::Buffer proj_data_buffer;

  std::vector<Mesh> meshes;

  bool init_memory_allocator() {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physical_device;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    vmaCreateAllocator(&allocatorInfo, &allocator);
  }

  bool create_surface() {
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    // create blank SDL window for our application
    sdl_window =
        std::unique_ptr<SDL_Window, SDLWindowDestroyer>(SDL_CreateWindow(
            "Vulkan Engine",          // window title
            SDL_WINDOWPOS_UNDEFINED,  // window position x (don't care)
            SDL_WINDOWPOS_UNDEFINED,  // window position y (don't care)
            window_extent.width,      // window width in pixels
            window_extent.height,     // window height in pixels
            window_flags));
    SDL_SetWindowResizable(sdl_window.get(), SDL_TRUE);

    VkSurfaceKHR tmp_surface;
    if (!SDL_Vulkan_CreateSurface(sdl_window.get(), instance, &tmp_surface)) {
      std::cerr << "Failed to create VkSurface."
                << "\n";
      return false;
    }
    surface = vk::SurfaceKHR(tmp_surface);

    return true;
  }

  void create_global_descriptor_set() {
    vk::DescriptorPoolSize ps;
    ps.descriptorCount = 1;
    ps.setType(vk::DescriptorType::eUniformBuffer);

    vk::DescriptorPoolCreateInfo pci;
    pci.maxSets = 1;
    pci.poolSizeCount = 1;
    pci.pPoolSizes = &ps;
    auto descrip_pool = device.createDescriptorPool(pci);

    vk::DescriptorSetAllocateInfo ali;
    ali.descriptorPool = descrip_pool;
    ali.descriptorSetCount = 1;
    ali.pSetLayouts = &global_descriptor_layout;

    global_desc_set = device.allocateDescriptorSets(ali)[0];

    vk::BufferCreateInfo bci;
    bci.usage = vk::BufferUsageFlagBits::eUniformBuffer;
    bci.sharingMode = vk::SharingMode::eExclusive;
    bci.size = sizeof(ProjectionData);

    VmaAllocationCreateInfo aci = {};
    aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VkBuffer tmp_buffer;
    VmaAllocation alloc_info;
    VkBufferCreateInfo bci_c = static_cast<VkBufferCreateInfo>(bci);
    vmaCreateBuffer(allocator, &bci_c, &aci, &tmp_buffer, &alloc_info, nullptr);

    proj_data_buffer = vk::Buffer(tmp_buffer);

    // now bind this to a position where it can be written
    vmaMapMemory(allocator, alloc_info, &proj_data_ptr);

    vk::DescriptorBufferInfo bi;
    bi.offset = 0;
    bi.buffer = proj_data_buffer;
    bi.range = sizeof(ProjectionData);

    vk::WriteDescriptorSet w;
    w.descriptorCount = 1;
    w.descriptorType = vk::DescriptorType::eUniformBuffer;
    w.dstBinding = 0;
    w.dstSet = global_desc_set;
    w.pBufferInfo = &bi;

    std::vector<vk::WriteDescriptorSet> writes{w};
    device.updateDescriptorSets(writes, {});
  }

  void create_global_descriptor_layout() {
    vk::DescriptorSetLayoutBinding b;
    b.binding = 0;
    b.descriptorCount = 1;
    b.descriptorType = vk::DescriptorType::eUniformBuffer;
    b.stageFlags = vk::ShaderStageFlagBits::eVertex;

    vk::DescriptorSetLayoutCreateInfo lci;
    lci.bindingCount = 1;
    lci.pBindings = &b;
    global_descriptor_layout = device.createDescriptorSetLayout(lci);
  }

  bool init_vulkan() {
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

    create_surface();

    vkb::PhysicalDeviceSelector selector{vkb_instance};
    auto phys_ret =
        selector.set_surface(surface)
            .set_minimum_version(1, 3)  // require a vulkan 1.3 capable device
            .require_present()
            .set_required_features_13(
                VkPhysicalDeviceVulkan13Features{.dynamicRendering = true})
            .select();
    if (!phys_ret) {
      std::cerr << "Failed to select Vulkan Physical Device. Error: "
                << phys_ret.error().message() << "\n";
      return false;
    }

    vkb::DeviceBuilder device_builder{phys_ret.value()};
    // automatically propagate needed data from instance & physical device
    auto dev_ret = device_builder.build();
    if (!dev_ret) {
      std::cerr << "Failed to create Vulkan device. Error: "
                << dev_ret.error().message() << "\n";
      return false;
    }
    vkb::Device vkb_device = dev_ret.value();

    // Get the VkDevice handle used in the rest of a vulkan application
    physical_device = vkb_device.physical_device.physical_device;
    device = vk::Device(vkb_device.device);

    // Get the graphics queue with a helper function
    auto graphics_queue_ret = vkb_device.get_queue(vkb::QueueType::graphics);
    if (!graphics_queue_ret) {
      std::cerr << "Failed to get graphics queue. Error: "
                << graphics_queue_ret.error().message() << "\n";
      return false;
    }
    graphics_queue = vk::Queue(graphics_queue_ret.value());
    graphics_queue_family =
        vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    create_command_pool();

    init_memory_allocator();

    create_swapchain();

    create_global_descriptor_layout();

    create_pipeline();

    create_global_descriptor_set();

    // tmp area for model loading

    meshes = {Mesh::load(
        "/home/malte/Documents/vscode/Vulkan3D/assets/bunny.obj",
        allocator)};

    auto test = meshes[0];

    return true;
  }

  void create_command_pool() {
    vk::CommandPoolCreateInfo cpi;
    cpi.queueFamilyIndex = graphics_queue_family;
    cpi.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    graphics_command_pool = device.createCommandPool(cpi);
  }

  vk::CommandBuffer create_graphics_command_buffer() {
    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandBufferCount = 1;
    alloc_info.commandPool = graphics_command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    return device.allocateCommandBuffers(alloc_info)[0];
  }

  void setup_swapchain_image_format() {
    auto surface_formates = physical_device.getSurfaceFormatsKHR(surface);

    bool found = false;
    for (auto surfaceFormat : surface_formates) {
      if (surfaceFormat == vk::Format::eB8G8R8A8Srgb) {
        found = true;
        break;
      }
    }
    if (!found) {
      throw std::runtime_error("vk::Format::eB8G8R8A8Srgb not available.");
    }

    swapchain_format = vk::Format::eB8G8R8A8Srgb;
  }

  void create_swapchain() {
    int sdl_width;
    int sdl_height;
    SDL_Vulkan_GetDrawableSize(sdl_window.get(), &sdl_width, &sdl_height);
    swapchain_extend = vk::Extent2D(sdl_width, sdl_height);

    setup_swapchain_image_format();

    vk::SwapchainCreateInfoKHR sci;
    sci.surface = surface;
    sci.imageSharingMode = vk::SharingMode::eExclusive;
    sci.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    sci.presentMode = vk::PresentModeKHR::eFifoRelaxed;
    sci.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    sci.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    sci.setImageExtent(swapchain_extend);
    sci.setMinImageCount(2);
    sci.imageArrayLayers = 1;
    sci.clipped = true;
    sci.setImageFormat(swapchain_format);
    sci.oldSwapchain = swapchain;
    swapchain = device.createSwapchainKHR(sci);

    swapchain_images = device.getSwapchainImagesKHR(swapchain);
    swapchain_image_views.reserve(swapchain_images.size());
    vk::ImageViewCreateInfo ivi;
    ivi.setFormat(swapchain_format);
    vk::ImageSubresourceRange sub_range;
    sub_range.aspectMask = vk::ImageAspectFlagBits::eColor;
    sub_range.baseArrayLayer = 0;
    sub_range.baseMipLevel = 0;
    sub_range.layerCount = 1;
    sub_range.levelCount = 1;
    ivi.setSubresourceRange(sub_range);
    ivi.viewType = vk::ImageViewType::e2D;

    for (auto image : swapchain_images) {
      ivi.image = image;
      swapchain_image_views.push_back(device.createImageView(ivi));
    }

    create_depth_attachment();
  }

  VkImageCreateInfo image_create_info(VkFormat format,
                                      VkImageUsageFlags usageFlags,
                                      VkExtent3D extent) {
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;

    info.imageType = VK_IMAGE_TYPE_2D;

    info.format = format;
    info.extent = extent;

    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usageFlags;

    return info;
  }

  void create_depth_attachment() {
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage depth_image;
    VmaAllocation depth_image_alloc_info;
    VkExtent3D depthImageExtent = {swapchain_extend.width,
                                   swapchain_extend.height, 1};
    VkImageCreateInfo ici_c = image_create_info(
        VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        depthImageExtent);
    vmaCreateImage(allocator, &ici_c, &alloc_info, &depth_image,
                   &depth_image_alloc_info, nullptr);

    vk::ImageViewCreateInfo ivci;
    ivci.format = vk::Format::eD32Sfloat;
    ivci.image = vk::Image(depth_image);
    ivci.viewType = vk::ImageViewType::e2D;
    ivci.subresourceRange = vk::ImageSubresourceRange{
        vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1,
    };
    depth_image_view = device.createImageView(ivci);
  }

  vk::Fence create_fence() {
    vk::FenceCreateInfo fci;
    fci.flags = vk::FenceCreateFlagBits::eSignaled;
    return device.createFence(fci);
  }

  void record_layout_transition(vk::CommandBuffer& cmd_buffer, vk::Image image,
                                vk::ImageLayout old_layout,
                                vk::ImageLayout new_layout,
                                vk::PipelineStageFlags src_stage,
                                vk::PipelineStageFlags dst_stage,
                                vk::AccessFlags src_access_mask,
                                vk::AccessFlags dst_access_mask,
                                vk::ImageAspectFlags aspect_mask) {
    vk::ImageMemoryBarrier img_mem_barrier;
    img_mem_barrier.srcAccessMask = src_access_mask;
    img_mem_barrier.dstAccessMask = dst_access_mask;
    img_mem_barrier.oldLayout = old_layout;
    img_mem_barrier.newLayout = new_layout;
    img_mem_barrier.setImage(image);

    vk::ImageSubresourceRange sub_range;
    sub_range.aspectMask = aspect_mask;
    sub_range.baseArrayLayer = 0;
    sub_range.baseMipLevel = 0;
    sub_range.layerCount = 1;
    sub_range.levelCount = 1;
    img_mem_barrier.subresourceRange = sub_range;

    std::vector<vk::ImageMemoryBarrier> img_mem_barriers{img_mem_barrier};
    cmd_buffer.pipelineBarrier(src_stage, dst_stage,
                               vk::DependencyFlagBits::eByRegion, {}, {},
                               img_mem_barriers);
  }

  std::vector<char> readFile(const std::string& filename) {
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

  vk::ShaderModule read_shader(const std::string& filename) {
    vk::ShaderModuleCreateInfo ci;
    auto shader_code = readFile(filename);
    ci.codeSize = shader_code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());
    return device.createShaderModule(ci);
  }

  vk::PipelineShaderStageCreateInfo create_shader_stage(
      const std::string& filename, const vk::ShaderStageFlagBits stage) {
    vk::PipelineShaderStageCreateInfo shader_info;
    shader_info.stage = stage;
    shader_info.module = read_shader(filename);
    shader_info.pName = "main";
    return shader_info;
  }

  void create_pipeline() {
    auto vertex_shader = create_shader_stage(
        "/home/malte/Documents/vscode/Vulkan3D/shaders/shader.vert.spv",
        vk::ShaderStageFlagBits::eVertex);
    auto frag_shader = create_shader_stage(
        "/home/malte/Documents/vscode/Vulkan3D/shaders/shader.frag.spv",
        vk::ShaderStageFlagBits::eFragment);

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages{vertex_shader,
                                                                 frag_shader};

    std::vector<vk::DynamicState> dynamic_states{vk::DynamicState::eViewport,
                                                 vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic_state_info({}, dynamic_states);

    vk::PipelineViewportStateCreateInfo viewport_state;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    vk::PipelineLayoutCreateInfo layout_ci;
    layout_ci.pushConstantRangeCount = 0;
    layout_ci.setLayoutCount = 1;
    layout_ci.pSetLayouts = &global_descriptor_layout;
    pipeline_layout = device.createPipelineLayout(layout_ci);

    vk::VertexInputBindingDescription vertex_binding_info;
    vertex_binding_info.binding = 0;
    vertex_binding_info.stride = sizeof(Vertex);
    vertex_binding_info.inputRate = vk::VertexInputRate::eVertex;

    vk::VertexInputAttributeDescription vert_pos_att_info;
    vert_pos_att_info.binding = 0;
    vert_pos_att_info.format = vk::Format::eR32G32B32Sfloat;
    vert_pos_att_info.location = 0;
    vert_pos_att_info.offset = 0;

    vk::VertexInputAttributeDescription vert_normal_att_info;
    vert_normal_att_info.binding = 0;
    vert_normal_att_info.format = vk::Format::eR32G32B32Sfloat;
    vert_normal_att_info.location = 1;
    vert_normal_att_info.offset = offsetof(Vertex, normal);

    vk::VertexInputAttributeDescription vert_texcoord_att_info;
    vert_texcoord_att_info.binding = 0;
    vert_texcoord_att_info.format = vk::Format::eR32G32Sfloat;
    vert_texcoord_att_info.location = 2;
    vert_texcoord_att_info.offset = offsetof(Vertex, tex_coord);

    std::vector<vk::VertexInputAttributeDescription> vert_att_descriptions{
        vert_pos_att_info, vert_normal_att_info, vert_texcoord_att_info};

    vk::PipelineVertexInputStateCreateInfo vertex_state;
    vertex_state.vertexBindingDescriptionCount = 1;
    vertex_state.pVertexBindingDescriptions = &vertex_binding_info;
    vertex_state.setVertexAttributeDescriptions(vert_att_descriptions);

    vk::PipelineInputAssemblyStateCreateInfo input_assembly_state;
    input_assembly_state.setTopology(vk::PrimitiveTopology::eTriangleList);

    vk::PipelineRasterizationStateCreateInfo rasterizer_info;
    rasterizer_info.lineWidth = 1.0f;
    rasterizer_info.depthClampEnable = false;
    rasterizer_info.rasterizerDiscardEnable = false;
    rasterizer_info.polygonMode = vk::PolygonMode::eFill;
    rasterizer_info.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer_info.frontFace = vk::FrontFace::eClockwise;
    rasterizer_info.depthBiasEnable = false;

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.sampleShadingEnable = false;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState color_attachment_state;
    color_attachment_state.colorWriteMask =
        vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB;
    color_attachment_state.blendEnable = false;

    vk::PipelineColorBlendStateCreateInfo color_blend;
    color_blend.logicOpEnable = false;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_attachment_state;

    vk::PipelineRenderingCreateInfo rendering_info;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = &swapchain_format;
    rendering_info.depthAttachmentFormat = vk::Format::eD32Sfloat;

    vk::PipelineDepthStencilStateCreateInfo depth_stencial_state;
    depth_stencial_state.depthTestEnable = true;
    depth_stencial_state.depthWriteEnable = true;
    depth_stencial_state.stencilTestEnable = false;
    depth_stencial_state.maxDepthBounds = 1.f;
    depth_stencial_state.minDepthBounds = 0.f;
    depth_stencial_state.depthCompareOp = vk::CompareOp::eLess;

    vk::GraphicsPipelineCreateInfo pipeline_create_info;
    pipeline_create_info.setStages(shader_stages);
    pipeline_create_info.layout = pipeline_layout;
    pipeline_create_info.pDynamicState = &dynamic_state_info;
    pipeline_create_info.pViewportState = &viewport_state;
    pipeline_create_info.pVertexInputState = &vertex_state;
    pipeline_create_info.pInputAssemblyState = &input_assembly_state;
    pipeline_create_info.pRasterizationState = &rasterizer_info;
    pipeline_create_info.pColorBlendState = &color_blend;
    pipeline_create_info.pNext = &rendering_info;
    pipeline_create_info.pDepthStencilState = &depth_stencial_state;

    auto pipeline_result =
        device.createGraphicsPipeline({}, pipeline_create_info);
    VK_CHECK(pipeline_result.result);
    pipeline = pipeline_result.value;
  }

  void recreate_swapchain() {
    device.waitIdle();
    create_swapchain();
  }

  void draw(vk::CommandBuffer& cmd_buffer, const SyncStructres& sync_struct,
            const int& frame_number) {
    cmd_buffer.reset();
    vk::CommandBufferBeginInfo cmd_begin_info;
    cmd_buffer.begin(cmd_begin_info);

    auto aquire_result_value = device.acquireNextImageKHR(
        swapchain, UINT64_MAX, {sync_struct.aquire_sem}, {});

    VK_CHECK(aquire_result_value.result);
    uint32_t swapchain_index = aquire_result_value.value;

    record_layout_transition(
        cmd_buffer, swapchain_images[swapchain_index],
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
        vk::ImageAspectFlagBits::eColor);

    vk::RenderingAttachmentInfoKHR color_att_info;
    color_att_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_att_info.imageView = swapchain_image_views[swapchain_index];
    color_att_info.loadOp = vk::AttachmentLoadOp::eClear;
    color_att_info.storeOp = vk::AttachmentStoreOp::eStore;
    color_att_info.setClearValue(vk::ClearValue({1.f, 1.f, 0.f, 1.f}));

    vk::RenderingAttachmentInfoKHR depth_att_info;
    depth_att_info.clearValue = vk::ClearValue({1, 0});
    depth_att_info.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depth_att_info.imageView = depth_image_view;
    depth_att_info.loadOp = vk::AttachmentLoadOp::eClear;
    depth_att_info.storeOp = vk::AttachmentStoreOp::eDontCare;

    vk::RenderingInfoKHR rendering_info;
    rendering_info.setColorAttachmentCount(1);
    rendering_info.setPColorAttachments(&color_att_info);
    rendering_info.layerCount = 1;
    rendering_info.setViewMask(0);
    rendering_info.setRenderArea(vk::Rect2D({0, 0}, swapchain_extend));
    rendering_info.pDepthAttachment = &depth_att_info;

    cmd_buffer.beginRendering(rendering_info);

    cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = swapchain_extend.width;
    viewport.height = swapchain_extend.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd_buffer.setViewport(0, 1, &viewport);

    cmd_buffer.setScissor(0, 1, &rendering_info.renderArea);

    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(
                     currentTime - startTime)
                     .count();

    auto model = glm::mat4(1);
    model = glm::scale(glm::vec3(8, 8, 8)) * model;
    model = glm::rotate(glm::radians(time * 45.f), glm::vec3(0, 1, 0)) * model;
    model = glm::translate(glm::vec3(0, 0, -3)) * model;
    auto view =
        glm::lookAt(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 0.0f, -3.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f));
    auto proj = glm::perspective(
        glm::radians(45.0f),
        swapchain_extend.width / (float)swapchain_extend.height, 0.1f, 10.0f);
    proj[1][1] *= -1;

    const auto to_view = view * model;
    const auto normal_matrix = glm::transpose(glm::inverse(to_view));
    const auto render_matrix = proj * to_view;
    ProjectionData proj_data{
        .render_mat = render_matrix,
        .normal_mat = normal_matrix,
    };

    VK_CHECK(
        device.waitForFences(1, &sync_struct.render_fence, true, UINT32_MAX));
    device.resetFences({sync_struct.render_fence});

    std::memcpy(proj_data_ptr, &proj_data, sizeof(ProjectionData));

    cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                  pipeline_layout, 0, 1, &global_desc_set, 0,
                                  nullptr);

    for (Mesh& mesh : meshes) {
      std::vector<vk::Buffer> vertex_buffers = {mesh.vertecies};
      std::vector<vk::DeviceSize> offsets = {0};
      cmd_buffer.bindVertexBuffers(0, vertex_buffers, offsets);

      cmd_buffer.bindIndexBuffer(mesh.indices, {0}, vk::IndexType::eUint32);

      cmd_buffer.drawIndexed(mesh.num_indices, 1, 0, 0, 0);
    }

    cmd_buffer.endRendering();

    record_layout_transition(cmd_buffer, swapchain_images[swapchain_index],
                             vk::ImageLayout::eColorAttachmentOptimal,
                             vk::ImageLayout::ePresentSrcKHR,
                             vk::PipelineStageFlagBits::eColorAttachmentOutput,
                             vk::PipelineStageFlagBits::eBottomOfPipe,
                             vk::AccessFlagBits::eColorAttachmentWrite,
                             vk::AccessFlagBits::eNone,
                             vk::ImageAspectFlagBits::eColor);

    cmd_buffer.end();

    vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo sub_info;
    sub_info.commandBufferCount = 1;
    sub_info.pCommandBuffers = &cmd_buffer;
    sub_info.waitSemaphoreCount = 1;
    sub_info.pWaitSemaphores = &sync_struct.aquire_sem;
    sub_info.pWaitDstStageMask = &wait_stage;
    sub_info.signalSemaphoreCount = 1;
    sub_info.pSignalSemaphores = &sync_struct.render_sem;
    graphics_queue.submit(sub_info, sync_struct.render_fence);

    graphics_queue.waitIdle();

    vk::PresentInfoKHR present_info;
    present_info.pImageIndices = &swapchain_index;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &sync_struct.render_sem;

    try {
      graphics_queue.presentKHR(present_info);
    } catch (vk::OutOfDateKHRError e) {
      recreate_swapchain();
      return;
    }

    graphics_queue.waitIdle();
  }

  vk::Semaphore create_semaphore() {
    vk::SemaphoreCreateInfo sci;
    return device.createSemaphore(sci);
  }

  void run() {
    SDL_Event e;
    bool bQuit = false;

    auto cmd_buffer = create_graphics_command_buffer();
    SyncStructres sync_structs {
      .render_fence = create_fence(),
      .aquire_sem = create_semaphore(),
      .render_sem = create_semaphore(),
    };

    int frame_number = 0;

    // main loop
    while (!bQuit) {
      // Handle events on queue
      while (SDL_PollEvent(&e) != 0) {
        // close the window when user clicks the X button or alt-f4s
        if (e.type == SDL_QUIT) bQuit = true;
      }

      draw(cmd_buffer, sync_structs, frame_number);
      frame_number += 1;
    }
  }

  ~TMP() {
    vkb::destroy_debug_utils_messenger(vkb_instance.instance,
                                       vkb_instance.debug_messenger);
  }
};

int main() {
  auto test = TMP();
  test.init_vulkan();
  test.run();
  return 0;
}