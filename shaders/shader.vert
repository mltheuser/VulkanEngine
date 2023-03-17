#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform MeshProjectionData {
    mat4 to_view;
    mat4 normal_to_view;
    mat4 to_screen;
} proj_data;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outNormal;

void main() {
    gl_Position = proj_data.to_screen * vec4(inPos, 1.0);
    outTexCoord = inTexCoord;
    outNormal = mat3(proj_data.normal_to_view) * inNormal;
}