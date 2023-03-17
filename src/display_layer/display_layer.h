#pragma once

#include <SDL.h>
#include <SDL_vulkan.h>

#include "../vulkan_layer/vulkan_layer.h"

#include <vector>
#include <memory>

struct SDLWindowDestroyer {
  void operator()(SDL_Window* w) const { SDL_DestroyWindow(w); }
};

class Display;

class SwapchainLayer {
 public:
  vk::SwapchainKHR swapchain;

  std::vector<ImageView> swapchain_image_views;

  ImageView depth_image_view;

  SwapchainLayer() {};
  SwapchainLayer(Display* parent_display);

  vk::Extent2D get_surface_extend();

  vk::Format get_swapchain_image_format();

 private:
  Display* parent_display;

  vk::SwapchainCreateInfoKHR swapchain_create_info();

  void create_swapchain_images();

  void create_swapchain();

  void create_depth_attachment();
};

class Display {
 public:
  std::unique_ptr<SDL_Window, SDLWindowDestroyer> sdl_window;
  vk::SurfaceKHR surface;

  SwapchainLayer swapchain;

  Display(const vk::Extent2D& size);

 private:
  bool create_surface(const vk::Extent2D& size);
};