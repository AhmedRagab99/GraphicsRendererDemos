// The same triangle shader written once in Slang. slangc compiles it to MSL (-> .metallib, Metal)
// and to SPIR-V (Vulkan). See triangle.metal and triangle.vert/.frag for the hand-written versions.

struct Params
{
    float4x4 transform;
};

// Vulkan: push constant. Metal: becomes a constant buffer argument.
[[vk::push_constant]] ConstantBuffer<Params> params;

static const float2 kPositions[3] = { float2(0.0, 0.6), float2(-0.52, -0.3), float2(0.52, -0.3) };
static const float3 kColors[3] = { float3(1.0, 0.25, 0.25), float3(0.25, 1.0, 0.25), float3(0.25, 0.4, 1.0) };

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

[shader("vertex")]
VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    output.position = mul(params.transform, float4(kPositions[vertexId], 0.0, 1.0));
    output.color = kColors[vertexId];
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}
