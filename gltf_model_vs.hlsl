#include "gltf_model.hlsli"

VS_OUT main(VS_IN vin)
{
    VS_OUT vout;
    vin.position.w = 1;
    vout.position = mul(vin.position, mul(world, view_projection));
    vout.w_position = mul(vin.position, world);

    vin.normal.w = 0;
    vout.w_normal = normalize(mul(vin.normal, world));

    float sigma = vin.tangent.w;
    vin.tangent.w = 0;
    vout.w_tangent = normalize(mul(vin.tangent, world));
    vout.w_tangent.w = sigma;
    vout.texcoord = vin.texcoord;
    return vout;
}
