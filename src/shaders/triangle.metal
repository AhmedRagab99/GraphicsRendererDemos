// MSL version of the triangle shader. The GLSL version (used by the Vulkan backend) is triangle.vert/.frag.
#include <metal_stdlib>
using namespace metal;

constant float2 kPositions[3] = {float2(0.0, 0.6), float2(-0.52, -0.3), float2(0.52, -0.3)};
constant float3 kColors[3] = {float3(1.0, 0.25, 0.25), float3(0.25, 1.0, 0.25), float3(0.25, 0.4, 1.0)};

struct VertexOut {
    float4 position [[position]];
    float3 color;
};

vertex VertexOut triangle_vertex(uint vertexId [[vertex_id]], constant float4x4& transform [[buffer(0)]]) {
    VertexOut out;
    out.position = transform * float4(kPositions[vertexId], 0.0, 1.0);
    out.color = kColors[vertexId];
    return out;
}

fragment float4 triangle_fragment(VertexOut in [[stage_in]]) {
    return float4(in.color, 1.0);
}
