#pragma once

#include "../vulkan_layer/vulkan_layer.h"

class Texture {
 public:
  ImageView view;
  vk::Sampler sampler;

  Texture(ImageView view, vk::Sampler sampler) : view{view}, sampler{sampler} {}

  static Texture load(const std::string& path);
};