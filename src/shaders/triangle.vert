#version 450
// GLSL version of the triangle shader, compiled to SPIR-V for the Vulkan backend.
// The MSL version (used by the Metal backend) is triangle.metal.

layout(push_constant) uniform PushConstants {
    mat4 transform;
} pc;

const vec2 kPositions[3] = vec2[](vec2(0.0, 0.6), vec2(-0.52, -0.3), vec2(0.52, -0.3));
const vec3 kColors[3] = vec3[](vec3(1.0, 0.25, 0.25), vec3(0.25, 1.0, 0.25), vec3(0.25, 0.4, 1.0));

layout(location = 0) out vec3 vColor;

void main() {
    gl_Position = pc.transform * vec4(kPositions[gl_VertexIndex], 0.0, 1.0);
    vColor = kColors[gl_VertexIndex];
}
