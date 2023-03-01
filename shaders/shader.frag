#version 450

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec4 outColor;

void main() {
    const int M = 10;
    float checker = float((mod(texCoord[0] * M, 1.0) > 0.5) ^^ (mod(texCoord[1] * M, 1.0) < 0.5));
    float c = 0.3 * (1 - checker) + 0.7 * checker;
    outColor = vec4(c, c, c, 1.0) * dot(normalize(normal), vec3(0, 0, 1));
}