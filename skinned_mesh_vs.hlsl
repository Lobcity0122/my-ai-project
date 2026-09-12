#include "skinned_mesh.hlsli"
VS_OUT main(VS_IN vin)
{
    VS_OUT vout;
    vout.position = mul(vin.position, mul(world, view_projection));
    vout.world_position = mul(vin.position, world);
    vin.normal.w = 0;
    vout.world_normal = normalize(mul(vin.normal, world));
    vout.texcoord = vin.texcoord;

#if 1
    vout.color = material_color;
#else
    vout.color = 0;
    const float4 bone_colors[4] = {
        {1, 0, 0, 1},
        {0, 1, 0, 1},
        {0, 0, 1, 1},
        {1, 1, 0, 1},
    };
    for (int bone_index = 0; bone_index < 4; ++bone_index)
    {
        vout.color += bone_colors[vin.bone_indices[bone_index] % 4]
            * vin.bone_weights[bone_index];
    }
#endif

    return vout;
}
