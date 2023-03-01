add_executable(vulkan3d main.cc)

target_include_directories(vulkan3d PUBLIC components)

target_include_directories(vulkan3d PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkan3d vkbootstrap vma glm tinyobjloader imgui stb_image)

target_link_libraries(vulkan3d Vulkan::Vulkan sdl2)

add_dependencies(vulkan3d Shaders)