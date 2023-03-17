add_library(vulkan_layer vulkan_layer/vulkan_layer.cc)
target_include_directories(vulkan_layer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(vulkan_layer PUBLIC vulkan_layer)
target_link_libraries(vulkan_layer Vulkan::Vulkan vkbootstrap vma)

add_library(display_layer display_layer/display_layer.cc)
target_include_directories(display_layer PUBLIC display_layer vulkan_layer)
target_link_libraries(display_layer vulkan_layer sdl2)

add_executable(vulkan3d main.cc components/mesh.cc)

target_include_directories(vulkan3d PUBLIC components)

target_link_libraries(vulkan3d vkbootstrap vma glm tinyobjloader imgui stb_image)

target_link_libraries(vulkan3d Vulkan::Vulkan sdl2)

target_link_libraries(vulkan3d vulkan_layer display_layer)

add_dependencies(vulkan3d Shaders)