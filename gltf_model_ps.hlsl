#include "gltf_model.hlsli"

struct texture_info
{
    int index;
    int texcoord;
};
struct normal_texture_info
{
    int index;
    int texcoord;
    float scale;
};
struct occlusion_texture_info
{
    int index;
    int texcoord;
    float strength;
};
struct pbr_metallic_roughness
{
    float4 basecolor_factor;
    texture_info basecolor_texture;
    float metallic_factor;
    float roughness_factor;
    texture_info metallic_roughness_texture;
};
struct material_constants
{
    float3 emissive_factor;
    int alpha_mode;
    float alpha_cutoff;
    int double_sided;
    pbr_metallic_roughness pbr_metallic_roughness;
    normal_texture_info normal_texture;
    occlusion_texture_info occlusion_texture;
    texture_info emissive_texture;
};
StructuredBuffer<material_constants> materials : register(t0);

#define BASECOLOR_TEXTURE 0
#define METALLIC_ROUGHNESS_TEXTURE 1
#define NORMAL_TEXTURE 2
#define EMISSIVE_TEXTURE 3
#define OCCLUSION_TEXTURE 4
Texture2D<float4> material_textures[5] : register(t1);

#define POINT 0
#define LINEAR 1
#define ANISOTROPIC 2
SamplerState sampler_states[3] : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    material_constants m = materials[material];
    float4 basecolor = m.pbr_metallic_roughness.basecolor_texture.index > -1 ?
        material_textures[BASECOLOR_TEXTURE].Sample(sampler_states[ANISOTROPIC], pin.texcoord) :
        m.pbr_metallic_roughness.basecolor_factor;
    float3 emmisive = m.emissive_texture.index > -1 ?
        material_textures[EMISSIVE_TEXTURE].Sample(sampler_states[ANISOTROPIC], pin.texcoord).rgb :
        m.emissive_factor;
    float3 N = normalize(pin.w_normal.xyz);
    float3 L = normalize(-light_direction.xyz);
    float3 color = max(0, dot(N, L)) * basecolor.rgb + emmisive;
    return float4(color, basecolor.a);
}
