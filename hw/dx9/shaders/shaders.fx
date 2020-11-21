/**
 * Standard shaders used for in-scene effects.
 *
 * Supported defines:
 * TEXTURE - enables texturing.
 * FOG_VERTEX, FOG_TABLE - enables fog of corresponding type.
 * MODULATE_ALPHA - switches to the MODULATE_ALPHA mode.
 */

/**
 * The texture.
 */
sampler2D base_texture : register(s0);

/**
 * Color of the fog.
 */
uniform float3 fog_color : register(c0);

/**
 * Fog range.
 *
 * (fog_start, fog_end)
 */
uniform float2 fog_range : register(c1);

/**
 * Size of the viewport in pixels.
 */
uniform float2 viewport_size : register(C2);

void vertex_main(
     float4 i_position : POSITION
    , out float4 o_position : POSITION

    , float4 i_diffuse : COLOR0
    , out float4 o_diffuse : COLOR0

#if defined(FOG_VERTEX) || defined(FOG_TABLE)
    , float4 i_specular : COLOR1
    , out float4 o_specular : COLOR1
#endif

#ifdef TEXTURE
    , float2 i_uv : TEXCOORD0
    , out float2 o_uv : TEXCOORD0
#endif
)
{

    const float w_coord = 1.0 / i_position.w;
    o_position = float4(
        (((i_position.x / viewport_size.x) - 0.5) * 2.0) * w_coord,
        (((i_position.y / viewport_size.y) - 0.5) * -2.0) * w_coord,
        i_position.z * w_coord,
        w_coord
    );
    o_diffuse = i_diffuse;

    // Disable alpha modulation by setting the vertex alpha to constant white.
    // Obviously if there is no texture, the blending mode does not apply so
    // keep the color unchanged.

#if defined(TEXTURE) && (! defined(MODULATE_ALPHA))
    o_diffuse.a = 1.0;
#endif // MODULATE_ALPHA

    // Optional UV.

#ifdef TEXTURE
    o_uv = i_uv;
#endif

    // Optional fog.

#if defined(FOG_VERTEX) || defined(FOG_TABLE)
    o_specular = i_specular;
#endif

}

float4 fragment_main(
      float4 diffuse : COLOR0
    , float4 specular_and_fog : COLOR1
#ifdef TEXTURE
    , float2 uv : TEXCOORD0
#endif
) : COLOR
{
    // Optional texture input.

#ifdef TEXTURE
    const float4 texel = tex2D(base_texture, uv);
#else
    const float4 texel = half4(1.0, 1.0, 1.0, 1.0);
#endif

    // Apply color.

    float4 result = texel;
    result.rgb *= diffuse.rgb;

#if defined(MODULATE_ALPHA) || (! defined(TEXTURE))
    result.a *= diffuse.a;
#endif

    // Apply fog.
    // TODO: Emulate the table fog properly.

#if defined(FOG_VERTEX) || defined(FOG_TABLE)
    result.rgb = lerp(fog_color.rgb, result.rgb, saturate(specular_and_fog.a));
#endif

    return result;
}

// EOF //
