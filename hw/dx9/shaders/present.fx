/**
 * Shader used for scene presentation effect.
 */

/**
 * The texture.
 */
sampler2D base_texture : register(s0);

void vertex_main(
     float3 i_position : POSITION
    , out float4 o_position : POSITION

    , float2 i_uv : TEXCOORD0
    , out float2 o_uv : TEXCOORD0
)
{
    o_position = float4(i_position.xy, 0.0, 1.0);
    o_uv = i_uv ;
}

float4 fragment_main(
    float2 uv : TEXCOORD0
) : COLOR
{
    return tex2D(base_texture, uv);
}

// EOF //
