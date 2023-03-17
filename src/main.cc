#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/transform.hpp>
#include <iostream>

#include "components/mesh.h"
#include "display_layer/display_layer.h"

struct SyncStructres {
  vk::Fence render_fence;
  vk::Semaphore aquire_sem;
  vk::Semaphore render_sem;
};

struct MeshPushConstants {
  glm::mat4 render_matrix;
};

class TMP {
 public:
  Display display = Display({1700, 800});

  vk::Pipeline pipeline;
  vk::PipelineLayout pipeline_layout;

  void* proj_data_ptr;
  Buffer proj_data_buffer;

  std::vector<Mesh> meshes;

  TMP() {
    create_pipeline();

    // tmp area for model loading
    meshes = {
        Mesh::load("/home/malte/Documents/vscode/Vulkan3D/assets/bunny.obj"),
        Mesh::load(
            "/home/malte/Documents/vscode/Vulkan3D/assets/viking_room.obj")};

    meshes[0].entity_to_world = glm::scale(glm::vec3(8, 8, 8)) * meshes[0].entity_to_world;
    meshes[0].entity_to_world = glm::translate(glm::vec3(1, 0, -3)) * meshes[0].entity_to_world;

    meshes[1].entity_to_world = glm::rotate(glm::radians(-90.f), glm::vec3(1, 0, 0)) * meshes[1].entity_to_world;
    meshes[1].entity_to_world = glm::rotate(glm::radians(-125.f), glm::vec3(0, 1, 0)) * meshes[1].entity_to_world;
    meshes[1].entity_to_world = glm::translate(glm::vec3(-1, 0, -3)) * meshes[1].entity_to_world;
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

  void create_pipeline() {
    auto vertex_shader = VulkanLayer::get_instance().create_shader_stage(
        "/home/malte/Documents/vscode/Vulkan3D/shaders/shader.vert.spv",
        vk::ShaderStageFlagBits::eVertex);
    auto frag_shader = VulkanLayer::get_instance().create_shader_stage(
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
    std::vector<vk::DescriptorSetLayout> desc_set_layouts{
        Mesh::get_descriptor_set_layout()};
    layout_ci.setSetLayouts(desc_set_layouts);
    pipeline_layout =
        VulkanLayer::get_instance().device.createPipelineLayout(layout_ci);

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

    vk::Format swapchain_format =
        display.swapchain.get_swapchain_image_format();
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
        VulkanLayer::get_instance().device.createGraphicsPipeline(
            {}, pipeline_create_info);
    VK_CHECK(pipeline_result.result);
    pipeline = pipeline_result.value;
  }

  void draw(vk::CommandBuffer& cmd_buffer, const SyncStructres& sync_struct,
            const int& frame_number) {
    VK_CHECK(VulkanLayer::get_instance().device.waitForFences(
        1, &sync_struct.render_fence, true, UINT32_MAX));
    VulkanLayer::get_instance().device.resetFences({sync_struct.render_fence});

    cmd_buffer.reset();
    vk::CommandBufferBeginInfo cmd_begin_info;
    cmd_buffer.begin(cmd_begin_info);

    auto aquire_result_value =
        VulkanLayer::get_instance().device.acquireNextImageKHR(
            display.swapchain.swapchain, UINT64_MAX, {sync_struct.aquire_sem},
            {});

    VK_CHECK(aquire_result_value.result);
    uint32_t swapchain_index = aquire_result_value.value;

    record_layout_transition(
        cmd_buffer,
        display.swapchain.swapchain_image_views[swapchain_index].image,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
        vk::ImageAspectFlagBits::eColor);

    vk::RenderingAttachmentInfoKHR color_att_info;
    color_att_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_att_info.imageView =
        display.swapchain.swapchain_image_views[swapchain_index].view;
    color_att_info.loadOp = vk::AttachmentLoadOp::eClear;
    color_att_info.storeOp = vk::AttachmentStoreOp::eStore;
    color_att_info.setClearValue(vk::ClearValue({1.f, 1.f, 0.f, 1.f}));

    vk::RenderingAttachmentInfoKHR depth_att_info;
    depth_att_info.clearValue = vk::ClearValue({1, 0});
    depth_att_info.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
    depth_att_info.imageView = display.swapchain.depth_image_view.view;
    depth_att_info.loadOp = vk::AttachmentLoadOp::eClear;
    depth_att_info.storeOp = vk::AttachmentStoreOp::eDontCare;

    auto swapchain_extend = display.swapchain.get_surface_extend();

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

    auto view =
        glm::lookAt(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 0.0f, -3.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f));
    auto proj = glm::perspective(
        glm::radians(45.0f),
        swapchain_extend.width / (float)swapchain_extend.height, 0.1f, 10.0f);
    proj[1][1] *= -1;

    for (Mesh& mesh : meshes) {
      auto old_mat = mesh.entity_to_world;
      mesh.entity_to_world = old_mat * glm::rotate(glm::radians(time * 45.f), glm::vec3(0, 1, 0));

      mesh.record_draw(cmd_buffer, pipeline_layout, view, proj);

      mesh.entity_to_world = old_mat;
    }

    cmd_buffer.endRendering();

    record_layout_transition(
        cmd_buffer,
        display.swapchain.swapchain_image_views[swapchain_index].image,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
        vk::ImageAspectFlagBits::eColor);

    cmd_buffer.end();

    vk::PipelineStageFlags wait_stage =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo sub_info;
    sub_info.commandBufferCount = 1;
    sub_info.pCommandBuffers = &cmd_buffer;
    sub_info.waitSemaphoreCount = 1;
    sub_info.pWaitSemaphores = &sync_struct.aquire_sem;
    sub_info.pWaitDstStageMask = &wait_stage;
    sub_info.signalSemaphoreCount = 1;
    sub_info.pSignalSemaphores = &sync_struct.render_sem;
    VulkanLayer::get_instance().graphics_queue.submit(sub_info,
                                                      sync_struct.render_fence);

    vk::PresentInfoKHR present_info;
    present_info.pImageIndices = &swapchain_index;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &display.swapchain.swapchain;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &sync_struct.render_sem;

    try {
      VulkanLayer::get_instance().graphics_queue.presentKHR(present_info);
    } catch (vk::OutOfDateKHRError e) {
      return;
    }
  }

  void run() {
    SDL_Event e;
    bool bQuit = false;

    auto cmd_buffer = VulkanLayer::get_instance().create_command_buffer(
        VulkanLayer::get_instance().graphics_command_pool);
    SyncStructres sync_structs{
        .render_fence = VulkanLayer::get_instance().create_fence(),
        .aquire_sem = VulkanLayer::get_instance().create_semaphore(),
        .render_sem = VulkanLayer::get_instance().create_semaphore(),
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
};

int main() {
  auto test = TMP();
  test.run();
  return 0;
}