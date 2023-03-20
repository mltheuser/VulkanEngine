#include "display_layer.h"

#include <iostream>

Display::Display(const vk::Extent2D& size) {
  create_surface(size);
  swapchain = SwapchainLayer(this);
}

bool Display::create_surface(const vk::Extent2D& size) {
  // We initialize SDL and create a window with it.
  SDL_Init(SDL_INIT_VIDEO);

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

  // create blank SDL window for our application
  sdl_window = std::unique_ptr<SDL_Window, SDLWindowDestroyer>(SDL_CreateWindow(
      "Vulkan3D",               // window title
      SDL_WINDOWPOS_UNDEFINED,  // window position x (don't care)
      SDL_WINDOWPOS_UNDEFINED,  // window position y (don't care)
      size.width,               // window width in pixels
      size.height,              // window height in pixels
      window_flags));
  SDL_SetWindowResizable(sdl_window.get(), SDL_TRUE);

  VkSurfaceKHR tmp_surface;
  if (!SDL_Vulkan_CreateSurface(sdl_window.get(), VulkanLayer::get_instance().instance, &tmp_surface)) {
    std::cerr << "Failed to create VkSurface."
              << "\n";
    return false;
  }
  surface = vk::SurfaceKHR(tmp_surface);

  return true;
}

SwapchainLayer::SwapchainLayer(Display* parent_display)
    : parent_display{parent_display} {
  create_swapchain();
}

vk::Extent2D SwapchainLayer::get_surface_extend() {
  int sdl_width;
  int sdl_height;
  SDL_Vulkan_GetDrawableSize(parent_display->sdl_window.get(), &sdl_width,
                             &sdl_height);
  return vk::Extent2D(sdl_width, sdl_height);
}

vk::SwapchainCreateInfoKHR SwapchainLayer::swapchain_create_info() {
  vk::SwapchainCreateInfoKHR sci;
  sci.surface = parent_display->surface;
  sci.imageSharingMode = vk::SharingMode::eExclusive;
  sci.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  sci.presentMode = vk::PresentModeKHR::eFifoRelaxed;
  sci.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  sci.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
  sci.setImageExtent(get_surface_extend());
  sci.setMinImageCount(2);
  sci.imageArrayLayers = 1;
  sci.clipped = true;
  sci.setImageFormat(get_swapchain_image_format());
  sci.oldSwapchain = swapchain;
  return sci;
}

void SwapchainLayer::create_swapchain_images() {
  auto swapchain_images =
      VulkanLayer::get_instance().device.getSwapchainImagesKHR(swapchain);
  swapchain_image_views.reserve(swapchain_images.size());
  for (auto image : swapchain_images) {
    Image wrapped_image(image);
    swapchain_image_views.push_back(
        VulkanLayer::get_instance().create_image_view(
            wrapped_image, get_swapchain_image_format(),
            vk::ImageAspectFlagBits::eColor));
  }
}

void SwapchainLayer::create_swapchain() {
  auto swapchain_extend = get_surface_extend();

  swapchain = VulkanLayer::get_instance().device.createSwapchainKHR(
      swapchain_create_info());

  create_swapchain_images();

  create_depth_attachment();
}

vk::Format SwapchainLayer::get_swapchain_image_format() {
  auto surface_formates =
      VulkanLayer::get_instance().physical_device.getSurfaceFormatsKHR(
          parent_display->surface);

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

  return vk::Format::eB8G8R8A8Srgb;
}

void SwapchainLayer::create_depth_attachment() {
  depth_image_view = VulkanLayer::get_instance().create_2d_image_view(
      get_surface_extend(), vk::Format::eD32Sfloat,
      vk::ImageUsageFlagBits::eDepthStencilAttachment,
      vk::ImageAspectFlagBits::eDepth, VMA_MEMORY_USAGE_GPU_ONLY);
}
