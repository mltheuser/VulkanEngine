#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Texture Texture::load(const std::string& path) {
  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels,
                              STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }

  auto staging_buffer = VulkanLayer::get_instance().create_buffer(
      imageSize, vk::BufferUsageFlagBits::eTransferSrc);

  void* staging_ptr;
  staging_buffer.map(staging_ptr);
  std::memcpy(staging_ptr, pixels, imageSize);
  staging_buffer.unmap();
  stbi_image_free(pixels);

  auto view = VulkanLayer::get_instance().create_2d_image_view(
      {texWidth, texHeight}, vk::Format::eR8G8B8A8Srgb,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::ImageAspectFlagBits::eColor, VMA_MEMORY_USAGE_GPU_ONLY);

  auto cpy_cmd_buffer = VulkanLayer::get_instance().create_command_buffer(
      VulkanLayer::get_instance().graphics_command_pool);

  vk::CommandBufferBeginInfo bi;
  cpy_cmd_buffer.begin(bi);

  vk::BufferImageCopy region;
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = vk::Offset3D();
  region.imageExtent = vk::Extent3D(texWidth, texHeight, 1);

  VulkanLayer::get_instance().record_layout_transition(
      cpy_cmd_buffer, view.image.image, vk::ImageLayout::eUndefined,
      vk::ImageLayout::eTransferDstOptimal,
      vk::PipelineStageFlagBits::eTopOfPipe,
      vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eNone,
      vk::AccessFlagBits::eTransferWrite, vk::ImageAspectFlagBits::eColor);

  cpy_cmd_buffer.copyBufferToImage(staging_buffer.buffer, view.image.image,
                                   vk::ImageLayout::eTransferDstOptimal, 1,
                                   &region);

  VulkanLayer::get_instance().record_layout_transition(
      cpy_cmd_buffer, view.image.image, vk::ImageLayout::eTransferDstOptimal,
      vk::ImageLayout::eShaderReadOnlyOptimal,
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eBottomOfPipe, vk::AccessFlagBits::eTransferWrite,
      vk::AccessFlagBits::eNone, vk::ImageAspectFlagBits::eColor);
      
  cpy_cmd_buffer.end();

  vk::SubmitInfo submit;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cpy_cmd_buffer;

  VulkanLayer::get_instance().graphics_queue.submit({submit});

  auto sampler = VulkanLayer::get_instance().create_sampler();

  return Texture(view, sampler);
}