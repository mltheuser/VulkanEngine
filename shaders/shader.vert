#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform ProjData {
    mat4 render_matrix;
    mat4 normal_mat;
} proj_data;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outNormal;

void main() {
    gl_Position = proj_data.render_matrix * vec4(inPos, 1.0);
    outTexCoord = inTexCoord;
    outNormal = mat3(proj_data.normal_mat) * inNormal;
}