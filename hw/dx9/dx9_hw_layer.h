#ifndef DX9_HW_LAYER_H
#define DX9_HW_LAYER_H

#include "../hw_layer.h"
#include <windows.h>
#include <d3d9.h>
#include <atlbase.h>

namespace emu {

const size_t DX9_SHADER_VARIANT_COUNT = 13;

/**
 * @brief Number of different formats supported by surface cache.
 */
const size_t SURFACE_CACHE_FORMAT_SLOTS = (HWFORMAT_R4G4B4A4 - HWFORMAT_R5G6B5 + 1);

/**
 * @brief Maximal size of power of two texture supported by surface cache.
 */
const size_t MAX_CACHED_SURFACE_SIZE = 512;

/**
 * @brief Number of slots corresponding to possible power-of-two sizes.
 */
const size_t SURFACE_CACHE_SIZE_SLOTS = 9;

const size_t SURFACE_CACHE_SLOTS = 1 + (SURFACE_CACHE_FORMAT_SLOTS * SURFACE_CACHE_SIZE_SLOTS * SURFACE_CACHE_SIZE_SLOTS);

/**
 * @brief DX9 implementation of the HW layer.
 *
 * Prints all calls to logKA.
 */
class DX9HWLayer : public HWLayer {

private:

    /**
     * @brief Dimensions of the render target.
     */
    size_t width;
    size_t height;

    /**
     * @brief API interface.
     *
     * If Vista+ extension is used, the value of this member
     * is the same as direct3d_ex.
     */
    CComPtr<IDirect3D9> direct3d;

    /**
     * @brief API interface for Vista+ extension.
     */
    CComPtr<IDirect3D9Ex> direct3d_ex;

    /**
     * @brief Indicates if 3D vision support sould be enabled.
     */
    bool vision_3d;

    /**
     * @brief Indicates if reads from the z-buffer are slow on this hw.
     */
    bool slow_zbuffer_readback;

    /**
     * @brief Maximal anisotropy value.
     */
    size_t max_anisotropy;

    /**
     * @brief Type of multisampling to use.
     */
    D3DMULTISAMPLE_TYPE multisample_type;

    /**
     * @brief Quality of the multisampling.
     */
    size_t multisample_quality;

    /**
     * @brief Device to use.
     *
     * If Vista+ extension is used, the value of this member
     * is the same as device_ex.
     */
    CComPtr<IDirect3DDevice9> device;

    /**
     * @brief Device for Vista+ extension.
     */
    CComPtr<IDirect3DDevice9Ex> device_ex;

    /**
     * @brief Render target for color rendering.
     */
    CComPtr<IDirect3DSurface9> default_color;

    /**
     * @brief Render target for depth rendering.
     */
    CComPtr<IDirect3DSurface9> default_depth;

    /**
     * @name Compiled shaders for various variations.
     */
    //@{
    CComPtr<IDirect3DVertexShader9> vertex_shaders[DX9_SHADER_VARIANT_COUNT];
    CComPtr<IDirect3DPixelShader9> fragment_shaders[DX9_SHADER_VARIANT_COUNT];
    //@}

private:

    /**
     * @brief Buffer used to store vertices uploaded by set_triangle_vertices()) call.
     */
    CComPtr<IDirect3DVertexBuffer9> vertex_buffer;

    /**
     * @brief Index of start of last block uploaded by set_triangle_vertices()
     */
    size_t vertex_data_start_index;

    /**
     * @brief Free position within the vertex buffer.
     */
    size_t vertex_buffer_free_index;

    /**
     * @brief Buffer used for temporary upload of indices during set_triangle_vertices() call.
     */
    CComPtr<IDirect3DIndexBuffer9> index_buffer;

    /**
     * @brief Free position inside the index buffer.
     */
    size_t index_buffer_free_index;

private:

    /**
     * @brief Information about HW surface.
     */
    struct HWSurfaceInfo {

        size_t width;
        size_t height;

        /**
         * @brief Height used for mono render-target surfaces.
         */
        size_t mono_height;

        /**
         * @brief Stride of the in-memory format.
         *
         * NOT stride of the hw format.
         */
        size_t stride;

        HWFormat format;
        bool render_target;

        /**
         * @brief Format of the created surface.
         */
        D3DFORMAT dx_format;

        // Standard texturing.

        /**
         * @brief The texture object.
         *
         * Can be NULL if this is a depth buffer surface.
         */
        CComPtr<IDirect3DTexture9> texture;

        /**
         * @brief Reference to its top-most level.
         */
        CComPtr<IDirect3DSurface9> surface_0;

        // Read/upload with CPU based 16<->32 conversion for any surface type.

        /**
         * @brief Memory texture used to upload/read this texture.
         *
         * Will point to the texture itself if upload can be handled
         * directly.
         */
        CComPtr<IDirect3DTexture9> transfer_texture;

        /**
         * @brief Level 0 of the transfer texture.
         */
        CComPtr<IDirect3DSurface9> transfer_surface_0;

        // Read with GPU 32->16 conversion for render targets.

        /**
         * @brief Render target texture used to convert the texture to 16 bit colors.
         *
         * Null for non-render target textures or if HW conversion is not used.
         */
        CComPtr<IDirect3DTexture9> read_16b_texture_rt;

        /**
         * @brief Texture used to read contenxt of the read_texture_rt.
         *
         * Null for non-render target textures or if HW conversion is not used.
         */
        CComPtr<IDirect3DTexture9> read_16b_texture;

        /**
         * @brief Level 0 of the read_16b_texture_rt.
         */
        CComPtr<IDirect3DSurface9> read_16b_rt_surface_0;

        /**
         * @brief Level 0 of the read_16b_texture.
         */
        CComPtr<IDirect3DSurface9> read_16b_surface_0;

        // Composition support.

        /**
         * @brief Texture used for render target composition.
         *
         * Null for non-render target textures.
         */
        CComPtr<IDirect3DTexture9> composition_texture;

        // MSAA support.

        /**
         * @brief Multisampled render target surface.
         *
         * Set to NULL if MSAA is not used on this surface or this is a depth surface.
         */
        CComPtr<IDirect3DSurface9> msaa_render_target;

        enum MSAASync {
            MSAA_SYNC_TEXTURE, // Valid copy is in the texture.
            MSAA_SYNC_RT,      // Valid copy is in the msaa_render_target.
            MSAA_SYNC_BOTH,    // Both copies are the same.
        };

        /**
         * @brief State of synchronization between the main texture
         * and the msaa_render_target.
         *
         * Ignored if msaa_render_target is NULL.
         */
        MSAASync msaa_sync;

        // Caching support.

        /**
         * @brief Index of slot in cache this surface is compatible with.
         *
         * If zero, the entry can not be cached.
         */
        size_t cache_slot;

        /**
         * @brief List containing surfaces of equivalent parameters in cache.
         */
        HWSurfaceInfo *next_in_cache;

        HWSurfaceInfo();
    };

    // Last set state.

    struct HWState {

        DepthTest depth_test;
        AlphaTest alpha_test;
        Blend alpha_blend;

        Fog fog_mode;
        unsigned int fog_color;

        bool flat;

        TextureBlend texture_blend;
        CComPtr<IDirect3DTexture9> texture;

        HWSurfaceInfo * color_info;
        HWSurfaceInfo * depth_info;

    public:

        HWState();
        HWState(const HWState &other);

        void reset(void);

    } state;

    /**
     * @brief Shader combination which is currently active.
     */
    int active_combination;

    /**
     * @brief Are we within BeginScene()/EndScene() pair?
     */
    bool scene_active;

public:

    struct CacheSlot {
        HWSurfaceInfo *head;
        HWSurfaceInfo *tail;
    };

    /**
     * @brief Cache of available surfaces.
     *
     * Slot 0 is not used.
     */
    CacheSlot cache[SURFACE_CACHE_SLOTS];

public:

    DX9HWLayer();
    virtual ~DX9HWLayer();

    virtual bool get_display_modes(DisplayModeList &modes);

    virtual bool initialize(const HWND window, const size_t width, const size_t height);

private:

    void detect_slow_z_readback(const size_t adapter);
    void detect_anisotropy(const size_t adapter, const D3DDEVTYPE device_type);
    void detect_msaa(const size_t adapter, const D3DDEVTYPE device_type);

public:

    virtual void deinitialize(void);

    virtual void begin_scene(void);
    virtual void end_scene(void);

    virtual HWSurfaceHandle create_surface(const size_t width, const size_t height, const HWFormat format, const void * const memory, const bool render_target);

private:

    HWSurfaceHandle create_depth_surface(const size_t width, const size_t height);

public:

    virtual void destroy_surface(const HWSurfaceHandle surface);
    virtual void update_surface(const HWSurfaceHandle surface, const void * const memory);
    virtual void read_surface(const HWSurfaceHandle surface, void * const memory);
    virtual void compose_render_target(const HWSurfaceHandle surface, const void * const memory, const float * const color_key);

private:

    void compose_or_update_render_target(HWSurfaceInfo &info, const void * const memory, const bool update, const float * const color_key);
    void update_render_target(HWSurfaceInfo &info, const void * const memory);

    void read_render_target(HWSurfaceInfo &info, void * const memory);
    void read_depth_surface(HWSurfaceInfo &info, void * const memory);

public:

    virtual void set_depth_test(const DepthTest test);
    virtual void set_alpha_test(const AlphaTest test);
    virtual void set_alpha_blend(const Blend blend);
    virtual void set_fog(const Fog fog, const unsigned int color);
    virtual void set_flat_blend(const bool enabled);
    virtual void set_texture_blend(const TextureBlend blend);
    virtual void set_texture_surface(const HWSurfaceHandle surface);

private:

    void set_depth_test_internal(const DepthTest test);
    void set_alpha_test_internal(const AlphaTest test);
    void set_alpha_blend_internal(const Blend blend);
    void set_fog_internal(const Fog fog, const unsigned int color);
    void set_flat_blend_internal(const bool enabled);
    void set_texture_blend_internal(const TextureBlend blend);
    void set_texture_surface_internal(const HWSurfaceInfo * const surface);

public:

    virtual void set_render_target(const HWSurfaceHandle color, const HWSurfaceHandle depth);
    virtual void clear(const RECT &rect, const bool color, const bool depth, const DWORD color_value, const float depth_value);
    virtual void set_triangle_vertices(const TLVertex * const vertices, const size_t count);
    virtual void draw_triangles(const TLVertex * const vertices, const size_t vertex_start, const size_t vertex_count, const unsigned short * const indices, const size_t triangle_count);
    virtual void draw_lines(const TLVertex * const vertices, const size_t vertex_start, const size_t vertex_count, const unsigned short * const indices, const size_t line_count);
    virtual void draw_points(const TLVertex * const vertices, const size_t vertex_start, const size_t vertex_count, const unsigned short * const indices, const size_t point_count);

    virtual void bitblt(const HWSurfaceHandle destination, const HWSurfaceHandle source, const size_t x, const size_t y, const size_t src_x, const size_t src_y, const size_t src_width, const size_t src_height);

    virtual void display_surface(const HWSurfaceHandle surface);

private:

    void synchronize_texture(HWSurfaceInfo &surface);
    void synchronize_msaa(HWSurfaceInfo &surface);
    void draw_fullscreen_quad(const size_t viewport_width, const size_t viewport_height);
    void draw_fullscreen_quad(const size_t viewport_width, const size_t viewport_height, const float txt_left, const float txt_top, const float txt_right, const float txt_bottom);
    void set_default_states(void);
    void activate_shader_combination(const int index);
    void apply_state(const HWState &state, const bool force);
    void bind_buffers(void);
    bool create_16bit_copy(HWSurfaceInfo &surface);

public:

    virtual void start_event(const wchar_t * const name);
    virtual void end_event(void);
    virtual void marker(const wchar_t * const name);
};

} // namespace

#endif // DX9_HW_LAYER_H

// EOF //
