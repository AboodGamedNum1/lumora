// HLSL twin of triangle.vert / triangle.frag, compiled to DXIL (shader model 6.0) by DXC.
// Direct3D clip space has +Y up, so the vertices are mirrored to give the same image as Vulkan.
float4 vs_main(uint id : SV_VertexID) : SV_Position {
  float2 p[3] = { float2(0.0, 0.8), float2(0.8, -0.8), float2(-0.8, -0.8) };
  return float4(p[id], 0.0, 1.0);
}
float4 ps_main() : SV_Target { return float4(1.0, 0.0, 0.0, 1.0); }
