/**
 * Shader used for scene composition effect.
 *
 * Must be run with not_equal 0 alpha test.
 *
 * If NON_BLACK_COLOR_KEY define is provided, the alpha is calculated
 * as distance from color_key_color constant. Otherwise it
 * is calculated to be zero for black color.
 */

/**
 * The texture.
 */
sampler2D base_texture : register(s0);

#ifdef NON_BLACK_COLOR_KEY

/**
 * @brief Value of color key.
 */
uniform float3 color_key_color : register(c10);

#endif

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
    const float4 color = tex2D(base_texture, uv);

#ifdef NON_BLACK_COLOR_KEY

    return float4(color.rgb, dot(abs(color.rgb - color_key_color), float3(1.0, 1.0, 1.0)));

#else //NON_BLACK_COLOR_KEY

    return float4(color.rgb, dot(color.rgb, float3(1.0, 1.0, 1.0)));

#endif //NON_BLACK_COLOR_KEY
}

// EOF //
