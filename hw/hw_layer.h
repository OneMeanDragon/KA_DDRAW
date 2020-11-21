#ifndef HW_LAYER_H
#define HW_LAYER_H

//#include <cstdlib> //<stdlib.h>
#include <windows.h>
#include <cstdlib> //<stdlib.h>
#include <list>
#include "../helpers/common.h"

namespace emu {

enum HWFormat {
    HWFORMAT_NONE, // Used to indicate error states.
    HWFORMAT_R5G6B5,
    HWFORMAT_R4G4B4A4,
    HWFORMAT_ZBUFFER,

    SIZE_OF_HWFORMAT,
};

enum AlphaTest {
    ALPHA_TEST_NONE,
    ALPHA_TEST_NOEQUAL,
};

enum Blend {
    BLEND_NONE,
    BLEND_OVER,
    BLEND_ADD,
};

enum Fog {
    FOG_NONE,
    FOG_VERTEX,
    FOG_TABLE,

    SIZE_OF_FOG
};

enum TextureBlend {
    TEXTURE_BLEND_MODULATE,
    TEXTURE_BLEND_MODULATEALPHA
};

enum DepthTest {
    DEPTH_TEST_NONE,
    DEPTH_TEST_ON,
    DEPTH_TEST_NOZWRITE
};

typedef void * HWSurfaceHandle;
const HWSurfaceHandle INVALID_SURFACE_HANDLE = NULL;

/**
 * @brief Vertex passed to the triangle rendering function.
 */
struct TLVertex {
    float sx;
    float sy;
    float sz;
    float rhw;
    DWORD color;
    DWORD specular;
    float tu;
    float tv;
};

/**
 * @brief Description of single display mode.
 */
struct DisplayMode {

    size_t width;
    size_t height;
    size_t refresh_rate;

    DisplayMode(const size_t the_width, const size_t the_height, const size_t the_refresh_rate)
        : width(the_width)
        , height(the_height)
        , refresh_rate(the_refresh_rate)
    {
    }
};

typedef std::list<DisplayMode> DisplayModeList;

/**
 * @brief Interface to target HW.
 *
 * Limited to features required by Klingon Academy.
 */
class HWLayer {

public:

    virtual ~HWLayer() {};

    /**
     * @brief Returns list of available display modes.
     *
     * Can be called on non-initialized device.
     */
    virtual bool get_display_modes(DisplayModeList &modes) = 0;

    // Global initialization.

    /**
     * @brief Initializes the api in full screen mode.
     *
     * @param width Width of the resolution.
     * @param height Height of the resolution.
     */
    virtual bool initialize(const HWND window, const size_t width, const size_t height) = 0;

    /**
     * @brief Deinitializes the api
     *
     * Can be called on object which was never initialized.
     *
     * @pre All surfaces must be destroyed.
     */
    virtual void deinitialize(void) = 0;

    // Drawing support.

    /**
     * @brief Called before any 3d drawing.
     */
    virtual void begin_scene(void) = 0;

    /**
     * @brief Called after 3d drawing.
     */
    virtual void end_scene(void) = 0;

    // Surface manipulation.

    /**
     * @brief Creates surface.
     *
     * @param width Width of the surface.
     * @param height Height of the surface.
     * @param format Format used for update and read of the surface.
     * @param memory Optional content to initialize the surface with.
     * @param render_target Will be render into this surface.
     */
    virtual HWSurfaceHandle create_surface(const size_t width, const size_t height, const HWFormat format, const void * const memory, const bool render_target) = 0;

    /**
     * @brief Destroys specified surface.
     */
    virtual void destroy_surface(const HWSurfaceHandle surface) = 0;

    /**
     * @brief Sets content of the surface from specified memory block.
     */
    virtual void update_surface(const HWSurfaceHandle surface, const void * const memory) = 0;

    /**
     * @brief Loads content of specified surface to specified memory block.
     */
    virtual void read_surface(const HWSurfaceHandle surface, void * const memory) = 0;

    /**
     * @brief Applies non-black pixels from memory over existing content of the render target.
     *
     * Can be called only outside scene and while no render target is bound.
     *
     * If color_key is not NULL, it must point to three float values used as color key for
     * surface transparency.
     */
    virtual void compose_render_target(const HWSurfaceHandle surface, const void * const memory, const float * const color_key) = 0;

    // State setup.

    /**
     * @brief Enables or disables use of the depth test.
     */
    virtual void set_depth_test(const DepthTest test) = 0;

    /**
     * @brief Sets alpha test to use.
     */
    virtual void set_alpha_test(const AlphaTest test) = 0;

    /**
     * @brief Sets blending mode to use.
     */
    virtual void set_alpha_blend(const Blend blend) = 0;

    /**
     * @brief Sets fog to use.
     */
    virtual void set_fog(const Fog fog, const unsigned int color) = 0;

    /**
     * @brief Enables flat blending instead of the gourard one.
     */
    virtual void set_flat_blend(const bool enabled) = 0;

    /**
     * @brief Sets how is the texture combined with vertex color.
     */
    virtual void set_texture_blend(const TextureBlend blend) = 0;

    /**
     * @brief Sets texture to use for rendering.
     */
    virtual void set_texture_surface(const HWSurfaceHandle surface) = 0;

    // Fixed state.
    // Alpha ref - 0
    // Alpha Stippling - false
    // Dither - On
    // Cull - D3DCULL_NONE
    // Fill mode - D3DFILL_SOLID 
    // Texture min - D3DFILTER_LINEAR
    // Texture mag -  D3DFILTER_LINEAR
    // Last pixel - FALSE
    //
    // Z func - D3DCMP_LESSEQUAL
    //
    // Fog Density = 1.0
    // Fog start = 0.5
    // Fog end = 1.0
    // Fog table mode - linear

    // Rendering.

    /**
     * @brief Sets specified surface and depth buffer as render target.
     *
     * Use null parameters to unset them.
     */
    virtual void set_render_target(const HWSurfaceHandle color, const HWSurfaceHandle depth) = 0;

    /**
     * @brief Clears color or depth buffer in specified rectangle.
     */
    virtual void clear(const RECT &rect, const bool color, const bool depth, const DWORD color_value, const float depth_value) = 0;

    /**
     * @brief Sets vertices which are referenced by the draw_triangles call.
     *
     * The array is provided again to the drawing function so the implementation can ignore this call.
     */
    virtual void set_triangle_vertices(const TLVertex * const vertices, const size_t count) = 0;

    /**
     * @brief Draws specified triangles.
     *
     * The vertices array points to the same vertices which were provided to last set_triangle_vertices()
     * call.
     */
    virtual void draw_triangles(const TLVertex * const vertices, const size_t vertex_start, const size_t vertex_count, const unsigned short * const indices, const size_t triangle_count) = 0;

    /**
     * @brief Draws specified vertices.
     *
     * The vertices array points to the same vertices which were provided to last set_triangle_vertices()
     * call.
     */
    virtual void draw_lines(const TLVertex * const vertices, const size_t vertex_start, const size_t vertex_count, const unsigned short * const indices, const size_t line_count) = 0;

    /**
     * @brief Draw points at specified positions.
     */
    virtual void draw_points(const TLVertex * const vertices, const size_t vertex_start, const size_t vertex_count, const unsigned short * const indices, const size_t point_count) = 0;

    /**
     * @brief Blits source rectangle from source surface to corresponding rectangle in the target surface.
     */
    virtual void bitblt(const HWSurfaceHandle destination, const HWSurfaceHandle source, const size_t x, const size_t y, const size_t src_x, const size_t src_y, const size_t src_width, const size_t src_height) = 0;

    // Presentation.

    /**
     * @brief Renders the surface into the device back buffer and flips it to the screen.
     */
    virtual void display_surface(const HWSurfaceHandle surface) = 0;

    // Debugging.

    /**
     * @brief Starts debugging event until paired end_event() is called.
     */
    virtual void start_event(const wchar_t * const UNUSED_PARAMETER(name)) {};

    /**
     * @brief Ends topmost debugging event started by start_event().
     */
    virtual void end_event(void) {};

    /**
     * @brief Marks single point in time.
     */
    virtual void marker(const wchar_t * const UNUSED_PARAMETER(name)) {};
};

/**
 * @brief Creates debug event on specified layer for lifetime of this object.
 */
class HWEventGuard {

    private:

        HWLayer &hw_layer;

    public:

        HWEventGuard(HWLayer &the_hw_layer, const wchar_t * const name) : hw_layer(the_hw_layer) { hw_layer.start_event(name); }
        ~HWEventGuard() { hw_layer.end_event(); }
};


/**
 * @brief Combines two tokens into one.
 */
#define CONCAT(first, second) first##second

// Enable HW events (PIX output) in the debug build.

#ifdef _DEBUG
#define HW_EVENTS
#endif

/**
 * @brief Creates HWEventGuard object with specified parameters and safe name.
 */
#ifdef HW_EVENTS
#define HWEVENT(layer, name) HWEventGuard CONCAT(guard,__LINE__)((layer), (name));
#else
#define HWEVENT(layer, name)
#endif

} // namespace emu

#endif // HW_LAYER_H

// EOF //
