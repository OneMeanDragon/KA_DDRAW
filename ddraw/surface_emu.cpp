#include "surface_emu.h"
#include "execute_buffer_emu.h"
#include "structure_log.h"
#include <assert.h>
#include "../helpers/config.h"

namespace emu {
namespace {

/**
 * @name Key identifying transparent values for HW composition operation.
 */
//@{
// For Klingon Academy the black color is entirely sufficient.

const unsigned short KA_COMPOSITION_KEY_MEMORY = 0x0000;
const float * KA_COMPOSITION_KEY = NULL;

// For Starfleet Academy the black is valid color for many areas (radar image)
// and must be allowed. They also blends some images with the key color
// so using some strange color will cause incorrect blend. For that reason
// we use color which is one blue step from the pure black. While it might
// be still present in valid parts of the image, it appears to be largely
// unnoticeable.

const unsigned short SFA_COMPOSITION_KEY_MEMORY = 0x0001;
const float SFA_COMPOSITION_KEY[3] = {0.0f, 0.0f, 0.0322580636f};
//@}

const float * get_composition_key(void)
{
    return is_inside_sfad3d() ? SFA_COMPOSITION_KEY : KA_COMPOSITION_KEY;
}

unsigned short get_composition_key_memory(void)
{
    return is_inside_sfad3d() ? SFA_COMPOSITION_KEY_MEMORY : KA_COMPOSITION_KEY_MEMORY;
}

/**
 * @brief Currently supported texture formats.
 */
const DDPIXELFORMAT texture_formats[SIZE_OF_HWFORMAT] = {
    {0},
    {
        sizeof(DDPIXELFORMAT),  // dwSize
        DDPF_RGB,               // dwFlags
        0,                      // dwFourCC
        16,                     // dwRGBBitCount
        0x0000F800,             // dwRBitMask
        0x000007E0,             // dwGBitMask
        0x0000001F,             // dwBBitMask
        0x00000000,             // dwBBitMask
    },
    {
        sizeof(DDPIXELFORMAT),  // dwSize
        DDPF_RGB | DDPF_ALPHAPIXELS,  // dwFlags
        0,                      // dwFourCC
        16,                     // dwRGBBitCount
        0x00000F00,             // dwRBitMask
        0x000000F0,             // dwGBitMask
        0x0000000F,             // dwBBitMask
        0x0000F000,             // dwBBitMask
    }
};

/**
 * @brief Checks if specified memory block contains at least one nonzero byte.
 *
 * If the size is not multiple of four or if compare is disabled, function will behave
 * as if there is a nonzero byte.
 */
bool is_nonzero(const void * const memory, const size_t memory_size)
{
    if (! is_composition_compare_enabled()) {
        return true;
    }

    if ((memory_size % 4) != 0) {
        return true;
    }

    const size_t step_count = memory_size / 4;
    size_t nonzero_found;
    __asm {
        mov edi, memory
        mov ecx, step_count
        xor eax, eax
        repe scasd
        mov nonzero_found, ecx
    }

    return (nonzero_found != 0);
}


/**
 * @brief Id of timer used to schedule presentations if the
 * application is not calling locking functions fast enough.
 */
const DWORD PRESENT_UPDATE_TIMER_ID = 1111;

/**
 * @brief Called when timer message is delivered.
 */
VOID CALLBACK deliver_present_timer(HWND hwnd, UINT UNUSED_PARAMETER(message), UINT_PTR event_id, DWORD UNUSED_PARAMETER(time))
{
    if (event_id != PRESENT_UPDATE_TIMER_ID) {
        return;
    }

    // Prevent the timer from occuring until explicitly restarted.

    KillTimer(hwnd, event_id);

    // Advance the emulation.

    DirectDrawSurfaceEmu * const surface = reinterpret_cast<emu::DirectDrawSurfaceEmu *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (surface) {
        logKA(MSG_VERBOSE, 0, "Present timer triggered");
        surface->update_presentation_emulation();
    }
}

/**
 * @brief Creates timer window.
 */
HWND create_timer_window(const HINSTANCE instance, DirectDrawSurfaceEmu &surface)
{
    return CreateWindowA(
        "D3DEMUTimerWindowClass",
        "D3DEMUTimerWindow",
        0,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        instance,
        &surface
    );
}

} // anonymous namespace.

EmulationInfo::EmulationInfo(const HINSTANCE instance, DirectDrawSurfaceEmu &surface)
    : emulation_state(EMULATION_STATE_WAITING_FOR_TIME)
    , emulation_timeout_start(0)
    , timer_window(NULL)
{
    if (! is_option_enabled("D3DEMU_NO_TIMER")) {
        timer_window = create_timer_window(instance, surface);
        if (timer_window == NULL) {
            logKA(emu::MSG_ERROR, 0, "Unable to create helper window, present timer is not available");
        }
        else {
            logKA(emu::MSG_INFORM, 0, "Present timer is enabled - use D3DEMU_NO_TIMER to disable it");
        }
    }
    else {
        logKA(emu::MSG_INFORM, 0, "Present timer is disabled");
    }
}

EmulationInfo::~EmulationInfo()
{
    if (timer_window) {
        DestroyWindow(timer_window);
    }
}

/**
 * @brief Constructor.
 */
DirectDrawSurfaceEmu::RenderStateSet::RenderStateSet()
    : hash(0)
    , sequence_number(0)
{
    memset(states, 0, sizeof(states));
}

/**
 * @brief Sets value of specified state.
 *
 * Returns true if there was change.
 */
bool DirectDrawSurfaceEmu::RenderStateSet::set_rs_dw(const D3DRENDERSTATETYPE type, const DWORD new_value)
{
    assert(type < (sizeof(states) / sizeof(states[0])));

    // Set value if there was change.

    const DWORD old_value = states[type].value.dw_value;
    if (old_value == new_value) {
        return false;
    }
    states[type].value.dw_value = new_value;

    // Update the hash.

    hash -= static_cast<unsigned __int64>(type) * old_value;
    hash += static_cast<unsigned __int64>(type) * new_value;
    sequence_number++;
    return true;
}

/**
 * @brief Sets value of specified state to provided float.
 *
 * Returns true if there was change.
 */
bool DirectDrawSurfaceEmu::RenderStateSet::set_rs_float(const D3DRENDERSTATETYPE type, const float value)
{
    RenderStateInfo info;
    info.value.float_value = value;
    return set_rs_dw(type, info.value.dw_value);
}

/**
 * @brief Returns value for specified dword typed render state.
 */
size_t DirectDrawSurfaceEmu::RenderStateSet::get_rs_dw(const D3DRENDERSTATETYPE type) const
{
    assert(type < (sizeof(states) / sizeof(states[0])));
    return states[type].value.dw_value;
}

/**
 * @brief Returns value for specified boolean typed render state.
 */
bool DirectDrawSurfaceEmu::RenderStateSet::get_rs_bool(const D3DRENDERSTATETYPE type) const
{
    assert(type < (sizeof(states) / sizeof(states[0])));
    return states[type].value.dw_value != 0;
}

/**
 * @brief Returns value for specified float typed render state.
 */
float DirectDrawSurfaceEmu::RenderStateSet::get_rs_float(const D3DRENDERSTATETYPE type) const
{
    assert(type < (sizeof(states) / sizeof(states[0])));
    return states[type].value.float_value;
}

/**
 * @brief Directly compares states without any optimizations.
 */
bool DirectDrawSurfaceEmu::RenderStateSet::compare_states(const RenderStateSet &other) const
{
    return memcmp(states, other.states, sizeof(states)) == 0;
}

/**
 * @brief Determines if both sets represent the same state.
 */
bool DirectDrawSurfaceEmu::RenderStateSet::equals(const RenderStateSet &other) const
{
    // Hash mismatch means difference.

    if (hash != other.hash) {
        assert(! compare_states(other));
        return false;
    }

    // Compare the content.

    return compare_states(other);
}

/**
 * @brief Compares both sets.
 *
 * If they have the same sequence number, they are
 * considered the same so this method should be only used
 * to compare state with its older constant copy.
 */
bool DirectDrawSurfaceEmu::RenderStateSet::equals_with_sequence(const RenderStateSet &other) const
{
    // Same sequence number means that the states are equivalent under
    // the assumption documented above.

    if (sequence_number == other.sequence_number) {
        assert(equals(other));
        return true;
    }

    // Full scale check.

    return equals(other);
}

/**
 * @brief Applies the render states.
 */
void DirectDrawSurfaceEmu::RenderStateSet::apply_render_states(HWLayer &hw_layer) const
{
    HWEVENT(hw_layer, L"apply_render_states");

    // Set complex states.

    hw_layer.set_depth_test(get_depth_test_state());
    hw_layer.set_alpha_test(get_alpha_test_state());
    hw_layer.set_alpha_blend(get_alpha_blend_state());
    hw_layer.set_fog(get_fog_state(), get_rs_dw(D3DRENDERSTATE_FOGCOLOR));
    hw_layer.set_flat_blend((get_rs_dw(D3DRENDERSTATE_SHADEMODE) & 0x0F) == D3DSHADE_FLAT);
    hw_layer.set_texture_blend(get_texture_blend());

    // Bind the texture. Will force upload to HW if necessary.

    DirectDrawSurfaceEmu * const texture = reinterpret_cast<DirectDrawSurfaceEmu *>(get_rs_dw(D3DRENDERSTATE_TEXTUREHANDLE));
    if (texture) {
        hw_layer.set_texture_surface(texture->get_hw_surface(false));
    }
    else {
        hw_layer.set_texture_surface(INVALID_SURFACE_HANDLE);
    }

    // Report mismatch in hardcoded values.

    if (get_rs_dw(D3DRENDERSTATE_CULLMODE) != D3DCULL_NONE) {
        logKA(MSG_ERROR, 0, "CULLMODE %u is not supported", get_rs_dw(D3DRENDERSTATE_CULLMODE));
    }
    if (get_rs_dw(D3DRENDERSTATE_FILLMODE) != D3DFILL_SOLID) {
        logKA(MSG_ERROR, 0, "FILLMODE %u is not supported", get_rs_dw(D3DRENDERSTATE_FILLMODE));
    }
    if (get_rs_bool(D3DRENDERSTATE_LASTPIXEL)) {
        logKA(MSG_ERROR, 0, "LASTPIXEL true is not supported");
    }
    if (get_rs_bool(D3DRENDERSTATE_STIPPLEDALPHA)) {
        logKA(MSG_ERROR, 0, "STIPPLEDALPHA true is not supported");
    }
    if (get_rs_dw(D3DRENDERSTATE_TEXTUREMAG) != D3DFILTER_LINEAR) {
        logKA(MSG_ERROR, 0, "TETUREMAG %u is not supported", get_rs_dw(D3DRENDERSTATE_TEXTUREMAG));
    }
    if (get_rs_dw(D3DRENDERSTATE_TEXTUREMIN) != D3DFILTER_LINEAR) {
        logKA(MSG_ERROR, 0, "TEXTUREMIN %u is not supported", get_rs_dw(D3DRENDERSTATE_TEXTUREMIN));
    }

    // States which we do not care for.
    //
    // D3DRENDERSTATE_DITHERENABLE
    // D3DRENDERSTATE_TEXTUREPERSPECTIVE
}

/**
 * @name Getters for HW states from DX states.
 *
 * Return default value and logKA the problem if the DX state is not supported.
 */
//@{
DepthTest DirectDrawSurfaceEmu::RenderStateSet::get_depth_test_state(void) const
{
    if (! get_rs_bool(D3DRENDERSTATE_ZENABLE)) {
        return DEPTH_TEST_NONE;
    }

    // The 'always' render state effectively disables test. If the
    // writes are disabled as well, the result is equivalent to the
    // full disable state (stencil buffer is not supported).

    if ((get_rs_dw(D3DRENDERSTATE_ZFUNC) == D3DCMP_ALWAYS) && (! get_rs_bool(D3DRENDERSTATE_ZWRITEENABLE))) {
        return DEPTH_TEST_NONE;
    }

    // For enabled state check that the function is right.

    if (get_rs_dw(D3DRENDERSTATE_ZFUNC) != D3DCMP_LESSEQUAL) {
        logKA(MSG_ERROR, 0, "ZFUNC %u is not supported", get_rs_dw(D3DRENDERSTATE_ZFUNC));
        return DEPTH_TEST_NONE;
    }

    // Check for disabled z-buffer writes.

    if (! get_rs_bool(D3DRENDERSTATE_ZWRITEENABLE)) {
        return DEPTH_TEST_NOZWRITE;
    }
    return DEPTH_TEST_ON;
}

AlphaTest DirectDrawSurfaceEmu::RenderStateSet::get_alpha_test_state(void) const
{
    if (! get_rs_bool(D3DRENDERSTATE_ALPHATESTENABLE)) {
        return ALPHA_TEST_NONE;
    }

    if (get_rs_dw(D3DRENDERSTATE_ALPHAREF) != 0) {
        logKA(MSG_ERROR, 0, "ALPHAREF %u is not supported", get_rs_dw(D3DRENDERSTATE_ALPHAREF));
        return ALPHA_TEST_NONE;
    }

    if (get_rs_dw(D3DRENDERSTATE_ALPHAFUNC) == D3DCMP_NOTEQUAL) {
        return ALPHA_TEST_NOEQUAL;
    }

    logKA(MSG_ERROR, 0, "ALPHAFUNC %u is not supported", get_rs_dw(D3DRENDERSTATE_ALPHAFUNC));
    return ALPHA_TEST_NONE;
}

Blend DirectDrawSurfaceEmu::RenderStateSet::get_alpha_blend_state(void) const
{
    if (! get_rs_bool(D3DRENDERSTATE_BLENDENABLE)) {
        return BLEND_NONE;
    }

    const size_t src = get_rs_dw(D3DRENDERSTATE_SRCBLEND);
    const size_t dest = get_rs_dw(D3DRENDERSTATE_DESTBLEND);
    if ((src == D3DBLEND_ONE) && (dest == D3DBLEND_ZERO)) {
        return BLEND_NONE;
    }
    if ((src == D3DBLEND_SRCALPHA) && (dest == D3DBLEND_INVSRCALPHA)) {
        return BLEND_OVER;
    }
    if ((src == D3DBLEND_BOTHSRCALPHA)) {
        return BLEND_OVER;
    }

    logKA(MSG_ERROR, 0, "Unsupported blend combination %u + %u", src, dest);
    return BLEND_NONE;
}

Fog DirectDrawSurfaceEmu::RenderStateSet::get_fog_state(void) const
{
    if (! get_rs_bool(D3DRENDERSTATE_FOGENABLE)) {
        return FOG_NONE;
    }

    // Fixed parameters.

    if (get_rs_float(D3DRENDERSTATE_FOGTABLEDENSITY) != 1.0f) {
        logKA(MSG_ERROR, 0, "FOGTABLEDENSITY %f is not supported", get_rs_float(D3DRENDERSTATE_FOGTABLEDENSITY));
        return FOG_NONE;
    }
    if (get_rs_float(D3DRENDERSTATE_FOGTABLEEND) != 1.0f) {
        logKA(MSG_ERROR, 0, "FOGTABLEEND %f is not supported", get_rs_float(D3DRENDERSTATE_FOGTABLEEND));
        return FOG_NONE;
    }
    if (get_rs_float(D3DRENDERSTATE_FOGTABLESTART) != 0.0f) {
        logKA(MSG_ERROR, 0, "FOGTABLESTART %f is not supported", get_rs_float(D3DRENDERSTATE_FOGTABLESTART));
        return FOG_NONE;
    }

    // Table mode.

    const size_t table_mode = get_rs_dw(D3DRENDERSTATE_FOGTABLEMODE);
    if (table_mode == D3DFOG_NONE) {
        return FOG_VERTEX;
    }
    if (table_mode == D3DFOG_LINEAR) {
        return FOG_TABLE;
    }
    logKA(MSG_ERROR, 0, "FOGTABLESTART %u is not supported", get_rs_float(D3DRENDERSTATE_FOGTABLESTART));
    return FOG_NONE;
}

TextureBlend DirectDrawSurfaceEmu::RenderStateSet::get_texture_blend(void) const
{
    const size_t mode = get_rs_dw(D3DRENDERSTATE_TEXTUREMAPBLEND);
    if (mode == D3DTBLEND_MODULATE) {
        return TEXTURE_BLEND_MODULATE;
    }
    if (mode == D3DTBLEND_MODULATEALPHA) {
        return TEXTURE_BLEND_MODULATEALPHA;
    }
    logKA(MSG_ERROR, 0, "TEXTUREMAPBLEND %u is not supported", mode);
    return TEXTURE_BLEND_MODULATE;
}
//@}

/**
 * @brief Constructor.
 */
DirectDrawSurfaceEmu::GeometryInfo::GeometryInfo()
    : geometry_mode(GEOMETRY_MODE_TRIANGLES)
    , indices()
    , min_vertex(~static_cast<size_t>(0))
    , max_vertex(0)
    , state_set()
{
}

/**
 * @brief Clears old content of the object with the exception of the state set.
 */
void DirectDrawSurfaceEmu::GeometryInfo::reset(void)
{
    geometry_mode = GEOMETRY_MODE_TRIANGLES;
    indices.clear();
    min_vertex = ~static_cast<size_t>(0);
    max_vertex = 0;
}

/**
 * @brief Determines if the geometry is empty.
 */
bool DirectDrawSurfaceEmu::GeometryInfo::is_empty(void) const
{
    return (indices.size() == 0);
}

/**
 * @brief Returns geometry mode.
 */
DirectDrawSurfaceEmu::GeometryMode DirectDrawSurfaceEmu::GeometryInfo::get_mode(void) const
{
    return geometry_mode;
}

/**
 * @brief Sets new geometry mode.
 *
 * The object must be empty.
 */
void DirectDrawSurfaceEmu::GeometryInfo::set_mode(const GeometryMode mode)
{
    assert(is_empty());
    geometry_mode = mode;
}

/**
 * @brief Sets state set to use for rendering of the future geometry.
 *
 * The object must be empty.
 */
void DirectDrawSurfaceEmu::GeometryInfo::set_state_set(const RenderStateSet &set)
{
    assert(is_empty());
    state_set = set;
}

/**
 * @brief Determines if specified set matches the set stored in the object.
 *
 * Function assumes that the set it got as parameter is some future copy
 * the set which was provided to the set_state_set().
 */
bool DirectDrawSurfaceEmu::GeometryInfo::is_state_set_unchanged(const RenderStateSet &set) const
{
    return state_set.equals_with_sequence(set);
}

/**
 * @brief Reads value from the D3DRENDERSTATE_SHADEMODE state.
 */
size_t DirectDrawSurfaceEmu::GeometryInfo::get_shade_mode_render_state(void) const
{
    return state_set.get_rs_dw(D3DRENDERSTATE_SHADEMODE);
}

/**
 * @brief Adds triangle with specified indices to the array of vertices.
 *
 * The object must be in the triangle mode.
 */
void DirectDrawSurfaceEmu::GeometryInfo::add_triangle(const unsigned short v0, const unsigned short v1, const unsigned short v2)
{
    assert(geometry_mode == GEOMETRY_MODE_TRIANGLES);

    // Store the indices.

    indices.push_back(v0);
    indices.push_back(v1);
    indices.push_back(v2);

    // Update limits.

    min_vertex = min(min_vertex, v0);
    min_vertex = min(min_vertex, v1);
    min_vertex = min(min_vertex, v2);

    max_vertex = max(max_vertex, v0);
    max_vertex = max(max_vertex, v1);
    max_vertex = max(max_vertex, v2);
}

/**
 * @brief Adds line with specified indices to the array of vertices.
 *
 * The object must be in the line mode.
 */
void DirectDrawSurfaceEmu::GeometryInfo::add_line(const unsigned short v0, const unsigned short v1)
{
    assert(geometry_mode == GEOMETRY_MODE_LINES);

    // Store the indices.

    indices.push_back(v0);
    indices.push_back(v1);

    // Update limits.

    min_vertex = min(min_vertex, v0);
    min_vertex = min(min_vertex, v1);

    max_vertex = max(max_vertex, v0);
    max_vertex = max(max_vertex, v1);
}

/**
 * @brief Add points using vertices from specified range.
 *
 * The object must be in the point mode.
 */
void DirectDrawSurfaceEmu::GeometryInfo::add_points(const size_t first, const size_t count)
{
    assert(geometry_mode == GEOMETRY_MODE_POINTS);
    if (count == 0) {
        return;
    }

    // Store the indices.

    assert(static_cast<unsigned short>(first + count - 1) == (first + count - 1));

    for (size_t i = 0; i < count; ++i) {
        indices.push_back(static_cast<unsigned short>(first + i));
    }

    // Update limits.

    min_vertex = min(min_vertex, first);
    max_vertex = max(max_vertex, (first + count - 1));
}

/**
 * @brief Applies the stored state to the hw.
 */
void DirectDrawSurfaceEmu::GeometryInfo::apply_state(HWLayer &hw_layer)
{
    state_set.apply_render_states(hw_layer);
}

/**
 * @brief Draws the stored geometry.
 *
 * Assumes that correct state is already set.
 */
void DirectDrawSurfaceEmu::GeometryInfo::draw_geometry(HWLayer &hw_layer, const TLVertex * const vertices)
{
    if (is_empty()) {
        return;
    }

    // Draw corresponding type of the geometry.

    switch (geometry_mode) {
        case GEOMETRY_MODE_TRIANGLES: {
            assert((indices.size() % 3) == 0);
            hw_layer.draw_triangles(
                vertices,
                min_vertex,
                (max_vertex - min_vertex) + 1,
                &indices[0],
                indices.size() / 3
            );
            break;
        }
        case GEOMETRY_MODE_LINES: {
            assert((indices.size() % 2) == 0);
            hw_layer.draw_lines(
                vertices,
                min_vertex,
                (max_vertex - min_vertex) + 1,
                &indices[0],
                indices.size() / 2
            );
            break;
        }
        case GEOMETRY_MODE_POINTS: {
            hw_layer.draw_points(
                vertices,
                min_vertex,
                (max_vertex - min_vertex) + 1,
                &indices[0],
                indices.size()
            );
            break;
        }
    }

    // Forget the geometry.

    reset();
}

DirectDrawSurfaceEmu::DirectDrawSurfaceEmu(HWLayer &the_hw_layer, const HINSTANCE the_instance)
    : hw_layer(the_hw_layer)
    , instance(the_instance)
    , desc()
    , master_surface(NULL)
    , owned(false)
    , attached_surfaces()
    , viewports()
    , memory(NULL)
    , hw_surface(INVALID_SURFACE_HANDLE)
    , master(MASTER_NONE)
    , emulation(NULL)
    , scene_active(false)
    , lock_count(0)
    , active_lock_hack(LOCK_HACK_NONE)
    , vertices()
{
    LOG_METHOD();
    master_surface = this;

    memset(supported_states, 0, sizeof(supported_states));
    set_default_render_states();

    queued_geometry.set_state_set(active_render_states);
    queued_overlay_geometry.set_state_set(active_render_states);
}

DirectDrawSurfaceEmu::~DirectDrawSurfaceEmu()
{
    LOG_METHOD();
    assert(master_surface == this);
    HWEVENT(hw_layer, L"~DirectDrawSurfaceEmu");

    if (hw_surface) {
        hw_layer.destroy_surface(hw_surface);
    }

    if (emulation) {
        kill_present_timer();
        delete emulation;
    }

    if (memory) {
        free(memory);
    }

    while (viewports.size() > 0) {
        DeleteViewport(viewports.front());
    }

    while (attached_surfaces.size() > 0) {
        detach_sub_surface(attached_surfaces.front());
    }
}

/**
 * @brief Attempt to initialize surface.
 */
HRESULT DirectDrawSurfaceEmu::initialize(const DDSURFACEDESC &descriptor)
{
    assert(memory == NULL);
    desc = descriptor;

    // Allocate the emulation structure if the type is right.

    if ((descriptor.ddsCaps.dwCaps & (DDSCAPS_COMPLEX | DDSCAPS_3DDEVICE | DDSCAPS_FRONTBUFFER)) == (DDSCAPS_COMPLEX | DDSCAPS_3DDEVICE | DDSCAPS_FRONTBUFFER)) {
        emulation = new EmulationInfo(instance, *this);
    }

    // Fill calculated values.

    desc.dwFlags |= DDSD_PITCH;

    if (get_hw_format() == HWFORMAT_ZBUFFER) {
        desc.lPitch = desc.dwWidth * 4;
    }
    else {
        desc.lPitch = desc.dwWidth * desc.ddpfPixelFormat.dwRGBBitCount / 8;
    }

    // Allocate system memory backing the surface.

    const size_t memory_size = desc.dwHeight * desc.lPitch;
    memory = malloc(memory_size);
    memset(memory, 0, memory_size);

    return DD_OK;
}

/**
 * @brief Attach owned surface to a complex surface.
 */
void DirectDrawSurfaceEmu::attach_sub_surface(DirectDrawSurfaceEmu * const surface, const bool as_owned)
{
    assert(desc.dwFlags & DDSD_CAPS);
    assert(desc.ddsCaps.dwCaps & DDSCAPS_COMPLEX);
    assert(surface);
    assert(surface->master_surface == surface);
    assert(! surface->owned);

    // TODO: Proper update of flags. The KA does not depend on exact emulation so not emulated.

    // Queue the surface.

    surface->master_surface = this;
    surface->owned = as_owned;
    surface->AddRef();
    attached_surfaces.push_back(surface);
}

/**
 * @brief Detaches specified subsurface.
 *
 * If the subsurface is owned, it will be destroyed.
 */
void DirectDrawSurfaceEmu::detach_sub_surface(DirectDrawSurfaceEmu * const surface)
{
    assert(surface);
    assert(surface->master_surface == this);

    // TODO: Proper update of flags. The KA does not depend on exact emulation so not emulated.

    // Remove the surface from the list.

    std::list<DirectDrawSurfaceEmu *>::iterator it;
    for (it = attached_surfaces.begin(); it != attached_surfaces.end(); ++it) {
        if (*it == surface) {
            attached_surfaces.erase(it);
            surface->master_surface = surface;

            // For non-owned surfaces release reference count.

            if (! surface->owned) {
                surface->Release();
            }
            else {

                // Owned surfaces are destroyed with us regardless of
                // number of references (DirectDraw<7 behavior).

                surface->owned = false;
                delete surface;
            }
            return;
        }
    }
}

/**
 * @brief Finds emulation info for this device.
 */
EmulationInfo *DirectDrawSurfaceEmu::find_emulation_info(void)
{
    if (master_surface == this) {
        return emulation;
    }
    else {
        return master_surface->find_emulation_info();
    }
}

/**
 * @brief Returns reference to the emulation info for situations where it is known
 * that the info must exist.
 */
EmulationInfo &DirectDrawSurfaceEmu::get_emulation_info(void)
{
    EmulationInfo * const info = find_emulation_info();
    assert(info);
    return *info;
}

/**
 * @brief Finds first surface with specified caps set.
 *
 * @param allow_up Allows traversal to master surfaces during lookup.
 */
DirectDrawSurfaceEmu * DirectDrawSurfaceEmu::find_surface(const size_t caps, const bool allow_up)
{
    // Are we the proper surface?

    if ((desc.ddsCaps.dwCaps & caps) == caps) {
        return this;
    }

    // Traverse to the root of the hierarchy.

    if (allow_up && (master_surface != this)) {
        return master_surface->find_surface(caps, true);
    }

    // Try the attached surfaces. This time do not allow upwards move.

    std::list<DirectDrawSurfaceEmu *>::iterator it;
    for (it = attached_surfaces.begin(); it != attached_surfaces.end(); ++it) {
        DirectDrawSurfaceEmu * const found = (*it)->find_surface(caps, false);
        if (found) {
            return found;
        }
    }

    return NULL;
}

/**
 * @brief Finds surface, if any, serving as front buffer.
 */
DirectDrawSurfaceEmu * DirectDrawSurfaceEmu::find_front_buffer(void)
{
    return find_surface(DDSCAPS_FRONTBUFFER);
}

/**
 * @brief Finds surface, if any, serving as backbuffer.
 */
DirectDrawSurfaceEmu * DirectDrawSurfaceEmu::find_back_buffer(void)
{
    return find_surface(DDSCAPS_BACKBUFFER);
}

/**
 * @brief Finds surface, if any, serving as depth buffer.
 */
DirectDrawSurfaceEmu * DirectDrawSurfaceEmu::find_depth_buffer(void)
{
    return find_surface(DDSCAPS_ZBUFFER);
}

/**
 * @brief Sets presentation timer to trigger after specified timeout.
 *
 * Does nothing if timer window is not available. The timer is triggered
 * only once per call to this function.
 */
void DirectDrawSurfaceEmu::set_present_timer(const size_t timeout)
{
    if (emulation && emulation->timer_window) {
        logKA(MSG_VERBOSE, 0, "Starting present timer for timeout %u", timeout);
        SetTimer(emulation->timer_window, PRESENT_UPDATE_TIMER_ID, timeout, deliver_present_timer);
    }
}

/**
 * @brief Kills presentation timer if it is active.
 */
void DirectDrawSurfaceEmu::kill_present_timer(void)
{
    if (emulation && emulation->timer_window) {
        logKA(MSG_VERBOSE, 0, "Killing present timer");
        KillTimer(emulation->timer_window, PRESENT_UPDATE_TIMER_ID);
    }
}

/**
 * @brief Monitors timeouts of emulation events.
 */
void DirectDrawSurfaceEmu::update_presentation_emulation(void)
{
    // Nothing to do for non-primary surfaces.

    if ((desc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) == 0) {
        return;
    }

    // Ignore sufraces which are still locked. We will be called again
    // at unlock time.

    if (lock_count > 0) {
        return;
    }

    // If the info is not present, we are used from the kamovies.exe so
    // show the surface.

    EmulationInfo * const info = find_emulation_info();
    if (info == NULL) {
        show_primary();
        return;
    }

    // Some states do not have timeout.

    if (
        (info->emulation_state == EmulationInfo::EMULATION_STATE_3D_SCENE) ||
        (info->emulation_state == EmulationInfo::EMULATION_STATE_3D_SCENE_POINT_GEOMETRY_DRAWN) ||
        (info->emulation_state == EmulationInfo::EMULATION_STATE_3D_SCENE_TRIANGLE_GEOMETRY_DRAWN) ||
        (info->emulation_state == EmulationInfo::EMULATION_STATE_3D_FLIP)
    ) {
        return;
    }

    // Select proper timeout.

    const size_t timeout_2d_mode = is_30fps_ui_enabled() ? 30 : 60;
    const size_t timeout =
        (info->emulation_state == EmulationInfo::EMULATION_STATE_WAITING_FOR_3D_SCENE) ? 300: timeout_2d_mode
    ;

    // Did it expire?

    const DWORD time = timeGetTime();
    const DWORD time_since_start = time - info->emulation_timeout_start;
    if (time_since_start < timeout) {
        set_present_timer(timeout - time_since_start);
        return;
    }

    // Update the mode and show the surface.

    info->emulation_timeout_start = time;
    info->emulation_state = EmulationInfo::EMULATION_STATE_WAITING_FOR_TIME;
    show_primary();

    // Restart the presentation timeout.

    set_present_timer(timeout_2d_mode);
}

/**
 * @brief Shows the surface if it is primary one.
 */
void DirectDrawSurfaceEmu::show_primary()
{
    // Nothing to do for non-primary surfaces.

    if ((desc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) == 0) {
        return;
    }
    HWEVENT(hw_layer, L"show_primary");

    // For primary, draw it to the screen.

    synchronize_hw();
    assert(hw_surface);
    hw_layer.display_surface(hw_surface);
}

/**
 * @brief Ensures that the memory buffer contains the latest data.
 */
void DirectDrawSurfaceEmu::synchronize_memory(void)
{
    assert(memory);
    if ((master == MASTER_NONE) || (master == MASTER_MEMORY) || (master == MASTER_SYNCHRONIZED)) {
        return;
    }
    HWEVENT(hw_layer, L"synchronize_memory");

    assert(hw_surface != INVALID_SURFACE_HANDLE);

    // If there is still a composition pending, we have to do it now.
    // This should not happen normally. The non-zero composition key
    // is used for Starfleet Academy which needs black parts of the image.
    // As the game runs in 640x480 resolution, the check is not implemented
    // and the worst possible situation is assumed.

    if (master == MASTER_COMPOSITION) {
        if ((get_composition_key_memory() != 0) || is_nonzero(memory, desc.dwHeight * desc.lPitch)) {
            hw_layer.compose_render_target(hw_surface, memory, get_composition_key());
        }
    }
    else if (master == MASTER_COMPOSITION_NONKEY) {
        hw_layer.compose_render_target(hw_surface, memory, get_composition_key());
    }

    // Read the result.

    hw_layer.read_surface(hw_surface, memory);
    master = MASTER_SYNCHRONIZED;
}

/**
 * @brief Ensures that the HW surface contains the latest data.
 *
 * Creates the HW surface if it does not exist yet.
 */
void DirectDrawSurfaceEmu::synchronize_hw(void)
{
    assert(memory);
    if ((master == MASTER_HW) || (master == MASTER_SYNCHRONIZED)) {
        return;
    }
    HWEVENT(hw_layer, L"synchronize_hw");

    // Create the surface if necessary. If the memory copy is
    // in fresh state, do not upload it yet as it is likely that
    // it will be filled soon.

    if (hw_surface == INVALID_SURFACE_HANDLE) {
        assert((master != MASTER_COMPOSITION) && (master != MASTER_COMPOSITION_NONKEY));
        void * const init_memory = (master == MASTER_NONE) ? NULL : memory;
        const bool render_target = (desc.ddsCaps.dwCaps & DDSCAPS_3DDEVICE) != 0;
        hw_surface = hw_layer.create_surface(desc.dwWidth, desc.dwHeight, get_hw_format(), init_memory, render_target);
        assert(hw_surface != INVALID_SURFACE_HANDLE);
        master = MASTER_SYNCHRONIZED;
    }
    else {
        if (master == MASTER_COMPOSITION) {
            if ((get_composition_key_memory() != 0) || is_nonzero(memory, desc.dwHeight * desc.lPitch)) {
                hw_layer.compose_render_target(hw_surface, memory, get_composition_key());
            }
            master = MASTER_HW;
        }
        else if (master == MASTER_COMPOSITION_NONKEY) {
            hw_layer.compose_render_target(hw_surface, memory, get_composition_key());
            master = MASTER_HW;
        }
        else {
            hw_layer.update_surface(hw_surface, memory);
            master = MASTER_SYNCHRONIZED;
        }
    }
}

/**
 * @brief Returns handle of the hardware surface ensuring that it is properly updated.
 *
 * If the for_rendering_into is true, the surface will be changed by the HW in the
 * future (the handle is used to bind it as framebuffer). Its master is changed
 * to reflect that the HW copy is now the master.
 */
HWSurfaceHandle DirectDrawSurfaceEmu::get_hw_surface(const bool for_rendering_into)
{
    synchronize_hw();
    if (hw_surface && for_rendering_into) {
        master = MASTER_HW;
    }
    return hw_surface;
}

/**
 * @brief Returns HW format corresponding to this surface.
 */
HWFormat DirectDrawSurfaceEmu::get_hw_format(void) const
{
    if (desc.ddsCaps.dwCaps & DDSCAPS_ZBUFFER) {
        return HWFORMAT_ZBUFFER;
    }

    for (size_t i = HWFORMAT_R5G6B5 ; i <= HWFORMAT_R4G4B4A4 ; ++i) {
        if (memcmp(texture_formats + i, &desc.ddpfPixelFormat, sizeof(DDPIXELFORMAT)) == 0) {
            return static_cast<HWFormat>(i);
        }
    }
    return HWFORMAT_NONE;
}

/**
 * @brief Sets default values for all render states.
 */
void DirectDrawSurfaceEmu::set_default_render_states(void)
{
    active_render_states = RenderStateSet();

    // Set values which are not 0 in the default situation.

    active_render_states.set_rs_dw(D3DRENDERSTATE_TEXTUREADDRESS, D3DTADDRESS_WRAP);
    active_render_states.set_rs_dw(D3DRENDERSTATE_SHADEMODE, D3DFILL_SOLID);
    active_render_states.set_rs_dw(D3DRENDERSTATE_FILLMODE, D3DSHADE_GOURAUD);
    active_render_states.set_rs_dw(D3DRENDERSTATE_PLANEMASK, ~static_cast<DWORD>(0));
    active_render_states.set_rs_dw(D3DRENDERSTATE_ZWRITEENABLE, 1);
    active_render_states.set_rs_dw(D3DRENDERSTATE_LASTPIXEL, 1);
    active_render_states.set_rs_dw(D3DRENDERSTATE_TEXTUREMAG, D3DFILTER_NEAREST);
    active_render_states.set_rs_dw(D3DRENDERSTATE_TEXTUREMIN, D3DFILTER_NEAREST);
    active_render_states.set_rs_dw(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
    active_render_states.set_rs_dw(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO);
    active_render_states.set_rs_dw(D3DRENDERSTATE_TEXTUREMAPBLEND, D3DTBLEND_MODULATE);
    active_render_states.set_rs_dw(D3DRENDERSTATE_CULLMODE, D3DCULL_CCW);
    active_render_states.set_rs_dw(D3DRENDERSTATE_ZFUNC, D3DCMP_LESSEQUAL);
    active_render_states.set_rs_dw(D3DRENDERSTATE_ALPHAFUNC, D3DCMP_ALWAYS);
    active_render_states.set_rs_dw(D3DRENDERSTATE_SPECULARENABLE, 1);
    active_render_states.set_rs_dw(D3DRENDERSTATE_FOGTABLEMODE, D3DFOG_NONE);

    // Not documented.

    active_render_states.set_rs_float(D3DRENDERSTATE_FOGTABLESTART, 0.0f);
    active_render_states.set_rs_float(D3DRENDERSTATE_FOGTABLEEND, 1.0f);
    active_render_states.set_rs_float(D3DRENDERSTATE_FOGTABLEDENSITY, 1.0f);

    // Mark states we support.

    memset(supported_states, 0, sizeof(supported_states));
    supported_states[D3DRENDERSTATE_ALPHAFUNC] = true;
    supported_states[D3DRENDERSTATE_ALPHAREF] = true;
    supported_states[D3DRENDERSTATE_ALPHATESTENABLE] = true;
    supported_states[D3DRENDERSTATE_BLENDENABLE] = true;
    supported_states[D3DRENDERSTATE_CULLMODE] = true;
    supported_states[D3DRENDERSTATE_DESTBLEND] = true;
    supported_states[D3DRENDERSTATE_DITHERENABLE] = true;
    supported_states[D3DRENDERSTATE_FILLMODE] = true;
    supported_states[D3DRENDERSTATE_FOGCOLOR] = true;
    supported_states[D3DRENDERSTATE_FOGENABLE] = true;
    supported_states[D3DRENDERSTATE_FOGTABLEDENSITY] = true;
    supported_states[D3DRENDERSTATE_FOGTABLEEND] = true;
    supported_states[D3DRENDERSTATE_FOGTABLEMODE] = true;
    supported_states[D3DRENDERSTATE_FOGTABLESTART] = true;
    supported_states[D3DRENDERSTATE_LASTPIXEL] = true;
    supported_states[D3DRENDERSTATE_SHADEMODE] = true;
    supported_states[D3DRENDERSTATE_SRCBLEND] = true;
    supported_states[D3DRENDERSTATE_STIPPLEDALPHA] = true;
    supported_states[D3DRENDERSTATE_TEXTUREHANDLE] = true;
    supported_states[D3DRENDERSTATE_TEXTUREMAG] = true;
    supported_states[D3DRENDERSTATE_TEXTUREMAPBLEND] = true;
    supported_states[D3DRENDERSTATE_TEXTUREMIN] = true;
    supported_states[D3DRENDERSTATE_TEXTUREPERSPECTIVE] = true;
    supported_states[D3DRENDERSTATE_ZENABLE] = true;
    supported_states[D3DRENDERSTATE_ZFUNC] = true;
    supported_states[D3DRENDERSTATE_ZWRITEENABLE] = true;
}

/**
 * @brief Sets specified render state.
 */
void DirectDrawSurfaceEmu::set_render_state(const D3DSTATE &state)
{
    // Check that the index is within the valid range.

    const D3DRENDERSTATETYPE type = state.drstRenderStateType;
    if (type >= RenderStateSet::RENDER_STATE_COUNT) {
        logKA(MSG_ERROR, 0, "Setting unknown or unsupported render state %u", type);
        return;
    }

    // Report changes of unsupported render states. Supported states are checked
    // for correct values during upload.

    if (! supported_states[type]) {
        logKA(MSG_ERROR, 0, "Setting unsupported render state %u", type);
    }

    // Apply the new value to the live copy.

    active_render_states.set_rs_dw(type, state.dwArg[0]);
}

/**
 * @brief Starts drawing of geometry with specified number of vertices.
 */
void DirectDrawSurfaceEmu::begin_geometry(const size_t count)
{
    assert(find_back_buffer() == this);
    queued_geometry.reset();
    queued_overlay_geometry.reset();
    vertices.resize(count);
}

/**
 * @brief Set specified range of vertices using specified source vertices.
 */
bool DirectDrawSurfaceEmu::set_vertices(const size_t start, const D3DTLVERTEX * const new_vertices, const size_t count)
{
    if (count == 0) {
        return true;
    }
    HWEVENT(hw_layer, L"set_vertices");

    // Check that the parameters make sense.

    if ((start + count) > vertices.size()) {
        logKA(MSG_ERROR, 0, "Attempting to set %u vertices from %u when only %u vertices should be present.", count, start, vertices.size());
        return false;
    }

    assert(sizeof(TLVertex) == sizeof(D3DTLVERTEX));
    const TLVertex * const input = reinterpret_cast<const TLVertex *>(new_vertices);

    // Copy the content.

    for (size_t i = 0; i < count; ++i) {
        vertices[start + i] = input[i];
    }

    // HACK: KA sets all vertices in single operation so we can upload
    // them directly to the HW for triangle drawing. We need to keep in
    // memory copy for point drawing.

    assert(count == vertices.size());
    hw_layer.set_triangle_vertices(&vertices[0], count);
    return true;
}

/**
 * @brief Adds triangle with specified indices to the array of vertices.
 */
void DirectDrawSurfaceEmu::add_triangle(const unsigned short v0, const unsigned short v1, const unsigned short v2)
{
    // If the overlay mode is active without correct underlying geometry, deactivate it.

    if (active_render_states.get_rs_dw(D3DRENDERSTATE_SHADEMODE) == GLOW_HACK_SHADING_MODE_OVERLAY) {
        if (queued_geometry.is_empty() || (queued_geometry.get_shade_mode_render_state() != GLOW_HACK_SHADING_MODE_BASE)) {
            logKA(MSG_ULTRA_VERBOSE, 0, "Switching from overlay mode because no underlying geometry is present");
            active_render_states.set_rs_dw(D3DRENDERSTATE_SHADEMODE, D3DSHADE_FLAT);
        }
    }

    // Select geometry object to use. This prevents the overlay triangle
    // from triggering flush of the base triangle queued before it.

    GeometryInfo &target_geometry =
        active_render_states.get_rs_dw(D3DRENDERSTATE_SHADEMODE) == GLOW_HACK_SHADING_MODE_OVERLAY ?
        queued_overlay_geometry :
        queued_geometry
    ;

    // Flush the geometry buffers if the mode does not match.

    if (target_geometry.get_mode() != GEOMETRY_MODE_TRIANGLES) {
        flush_geometry();
    }

    // Or if the state configuration changed since last time.

    if (! target_geometry.is_empty()) {
        if (! target_geometry.is_state_set_unchanged(active_render_states)) {
            flush_geometry();
        }
    }

    // If we are starting new batch of the geometry, set the mode and state
    // which will be used for the final drawing.

    if (target_geometry.is_empty()) {
        target_geometry.set_mode(GEOMETRY_MODE_TRIANGLES);
        target_geometry.set_state_set(active_render_states);
    }

    // Queue the triangle.

    target_geometry.add_triangle(v0, v1, v2);
}

/**
 * @brief Adds line using vertices from specified range.
 */
void DirectDrawSurfaceEmu::add_line(const size_t first, const size_t second)
{
    // The points are never part of the base or overlay geometry
    // so they will force flush of any pending overlay geometry.

    if (! queued_overlay_geometry.is_empty()) {
        flush_geometry();
    }

    // Flush the geometry buffers if the mode does not match.

    if (queued_geometry.get_mode() != GEOMETRY_MODE_LINES) {
        flush_geometry();
    }

    // Or if the state configuration changed since last time.

    if (! queued_geometry.is_empty()) {
        if (! queued_geometry.is_state_set_unchanged(active_render_states)) {
            flush_geometry();
        }
    }

    // If we are starting new batch of the geometry, set the mode and state
    // which will be used for the final drawing.

    if (queued_geometry.is_empty()) {
        queued_geometry.set_mode(GEOMETRY_MODE_LINES);
        queued_geometry.set_state_set(active_render_states);
    }

    // Queue the line.

    assert(static_cast<unsigned short>(first) == first);
    assert(static_cast<unsigned short>(second) == second);

    queued_geometry.add_line(static_cast<unsigned short>(first), static_cast<unsigned short>(second));
}

/**
 * @brief Add points using vertices from specified range.
 */
void DirectDrawSurfaceEmu::add_points(const size_t first, const size_t count)
{
    if (count == 0) {
        return;
    }

    // The points are never part of the base or overlay geometry
    // so they will force flush of any pending overlay geometry.

    if (! queued_overlay_geometry.is_empty()) {
        flush_geometry();
    }

    // Flush the geometry buffers if the mode does not match.

    if (queued_geometry.get_mode() != GEOMETRY_MODE_POINTS) {
        flush_geometry();
    }

    // Or if the state configuration changed since last time.

    if (! queued_geometry.is_empty()) {
        if (! queued_geometry.is_state_set_unchanged(active_render_states)) {
            flush_geometry();
        }
    }

    // If we are starting new batch of the geometry, set the mode and state
    // which will be used for the final drawing.

    if (queued_geometry.is_empty()) {
        queued_geometry.set_mode(GEOMETRY_MODE_POINTS);
        queued_geometry.set_state_set(active_render_states);
    }

    // Queue the points.

    queued_geometry.add_points(first, count);
}

/**
 * @brief Draws pending triangles/points.
 */
void DirectDrawSurfaceEmu::flush_geometry(void)
{
    assert(find_back_buffer() == this);

    // Nothing to draw if there is no geometry. Note that overlay geometry
    // is queued only when there is the base geometry.

    if (queued_geometry.is_empty()) {
        assert(queued_overlay_geometry.is_empty());
        return;
    }
    HWEVENT(hw_layer, L"flush_geometry");

    // Upload the back buffer to the HW if it was changed since last time.
    // The GPU copy will now become master.

    EmulationInfo &info = get_emulation_info();
    synchronize_hw();
    master = MASTER_HW;

    // Realize the rendering state for the base geometry.

    queued_geometry.apply_state(hw_layer);

    // Emulation specific hacks. Note that the KA enhancements will cause
    // the first geometry drawn by SFA to be drawn incorrectly so they
    // need to be disabled for it.

    if ((queued_geometry.get_mode() == GEOMETRY_MODE_TRIANGLES) && (! is_inside_sfad3d())) {

        // Apply the skybox override. Draw it in additive mode.

        if ((info.emulation_state == EmulationInfo::EMULATION_STATE_3D_SCENE) || (info.emulation_state == EmulationInfo::EMULATION_STATE_3D_SCENE_POINT_GEOMETRY_DRAWN)) {
            hw_layer.set_alpha_blend(BLEND_ADD);
            info.emulation_state = EmulationInfo::EMULATION_STATE_3D_SCENE_TRIANGLE_GEOMETRY_DRAWN;
        }
    }
    else {

        // Advance the emulation state.

        if (info.emulation_state == EmulationInfo::EMULATION_STATE_3D_SCENE) {
            info.emulation_state = EmulationInfo::EMULATION_STATE_3D_SCENE_POINT_GEOMETRY_DRAWN;
        }
    }

    // Draw it.

    queued_geometry.draw_geometry(hw_layer, &vertices[0]);
    assert(queued_geometry.is_empty());

    // Draw the overlay geometry if any.

    if (queued_overlay_geometry.is_empty()) {
        return;
    }

    queued_overlay_geometry.apply_state(hw_layer);
    queued_overlay_geometry.draw_geometry(hw_layer, &vertices[0]);
    assert(queued_overlay_geometry.is_empty());
}

/**
 * @brief Completes the geometry.
 *
 * Flushes any pending draws.
 */
void DirectDrawSurfaceEmu::end_geometry(void)
{
    assert(find_back_buffer() == this);
    flush_geometry();
    vertices.clear();
}

/**
 * @brief Attaches specified surface to end of chain for this surface.
 */
STDMETHODIMP DirectDrawSurfaceEmu::AddAttachedSurface(LPDIRECTDRAWSURFACE surface)
{
    LOG_METHOD();
    CHECK_NOT_NULL(surface);

    DirectDrawSurfaceEmu * const full_type = static_cast<DirectDrawSurfaceEmu *>(surface);
    if (full_type->master_surface != full_type) {
        return DDERR_SURFACEALREADYATTACHED;
    }

    attach_sub_surface(full_type, false);
    return DD_OK;
}

/**
 * @brief BltFast.
 */
STDMETHODIMP DirectDrawSurfaceEmu::BltFast(DWORD x,DWORD y,LPDIRECTDRAWSURFACE source, LPRECT rect,DWORD trans)
{
    LOG_METHOD();
    HWEVENT(hw_layer, L"BltFast");

    CHECK_NOT_NULL(source);
    logKA(MSG_VERBOSE, 1, "x: %d", x);
    logKA(MSG_VERBOSE, 1, "y: %d", y);
    logKA(MSG_VERBOSE, 1, "source: %p", source);
    if (rect) {
        logKA(MSG_VERBOSE, 1, "%dx%d->%dx%d", rect->left, rect->top, rect->right, rect->bottom);
    }
    switch (trans & 3) {
        case DDBLTFAST_DESTCOLORKEY : logKA(MSG_VERBOSE, 1, "DESTCOLORKEY"); break;
        case DDBLTFAST_SRCCOLORKEY : logKA(MSG_VERBOSE, 1, "SRCCOLORKEY"); break;
        case DDBLTFAST_NOCOLORKEY : logKA(MSG_VERBOSE, 1, "NOCOLORKEY"); break;
    }
    if (trans & DDBLTFAST_WAIT) {
        logKA(MSG_VERBOSE, 1, "WAIT");
    }

    // This function is not important for KA however necessary for SFA. Emulate
    // only required features.

    if (! is_inside_sfad3d()) {
        return DD_OK;
    }

    // Check for supported parameters.

    if ((trans & 3) != DDBLTFAST_NOCOLORKEY) {
        logKA(MSG_ERROR, 0, "Color key is not supported");
        return DDERR_UNSUPPORTED;
    }
    if (rect == NULL) {
        logKA(MSG_ERROR, 0, "BltFast no rectangle provided");
        return DDERR_INVALIDRECT;
    }
    if (
        (rect->left < 0) ||
        (rect->top < 0) ||
        (rect->left >= rect->right) ||
        (rect->top >= rect->bottom) ||
        (rect->right > static_cast<LONG>(desc.dwWidth)) ||
        (rect->bottom > static_cast<LONG>(desc.dwHeight))
    ) {
        logKA(MSG_ERROR, 0, "Incorrect or unsupported BltFast rectangle");
        return DDERR_INVALIDRECT;
    }

    DirectDrawSurfaceEmu * const src_surface = static_cast<DirectDrawSurfaceEmu *>(source);
    if (src_surface == this) {
        logKA(MSG_ERROR, 0, "Blit inside one surface is not supported");
        return DDERR_UNSUPPORTED;
    }

    // Ensure that the content is in the video memory. This allows us to do full
    // 24 bit blit.

    synchronize_hw();
    src_surface->synchronize_hw();

    // Execute the blit.

    hw_layer.bitblt(
        get_hw_surface(true),
        src_surface->get_hw_surface(false),
        x,
        y,
        rect->left,
        rect->top,
        rect->right - rect->left,
        rect->bottom - rect->top
    );

    return DD_OK;
}

/**
 * @brief Flips front and back buffer.
 */
STDMETHODIMP DirectDrawSurfaceEmu::Flip(LPDIRECTDRAWSURFACE surface_override, DWORD UNUSED_PARAMETER(flags))
{
    LOG_METHOD();

    if (surface_override) {
        logKA(MSG_ERROR, 0, "Flip: surface_override is not supported");
        return DDERR_UNSUPPORTED;
    }

    // Unable to do anything if we do not have both surfaces.

    DirectDrawSurfaceEmu * const front = find_front_buffer();
    DirectDrawSurfaceEmu * const back = find_back_buffer();
    if ((front == NULL) || (back == NULL)) {
        logKA(MSG_ERROR, 0, "Flip: Called on non-flippable surface.");
        return DDERR_NOTFLIPPABLE;
    }

    // Flip memory content of both surfaces.

    void * const tmp_memory = front->memory;
    front->memory = back->memory;
    back->memory = tmp_memory;

    const HWSurfaceHandle tmp_hw_surface = front->hw_surface;
    front->hw_surface = back->hw_surface;
    back->hw_surface = tmp_hw_surface;

    const Master tmp_master = front->master;
    front->master = back->master;
    back->master = tmp_master;

    // Ensure that the primary surface is updated if it is the
    // front buffer.

    front->show_primary();

    // Start wait for next BeginScene if running in the game.

    EmulationInfo * const info = find_emulation_info();
    if (info) {
        info->emulation_state = EmulationInfo::EMULATION_STATE_WAITING_FOR_3D_SCENE;
        info->emulation_timeout_start = timeGetTime();
    }
    return DD_OK;
}

/**
 * @brief Returns next attached surface with specified parameters.
 */
STDMETHODIMP DirectDrawSurfaceEmu::GetAttachedSurface(LPDDSCAPS caps, LPDIRECTDRAWSURFACE FAR * surface)
{
    LOG_METHOD();
    CHECK_NOT_NULL(caps);
    CHECK_NOT_NULL(surface);

    // logKA the request.

    log_structure(MSG_VERBOSE, 1, *caps);

    // Try to find next entry in the chain with specified attributes.

    DirectDrawSurfaceEmu * const found = find_surface(caps->dwCaps);
    if (found == NULL) {
        *surface = NULL;
        return DDERR_NOTFOUND;
    }

    // We found something only if we found anything else than self.

    if (found == this) {
        *surface = NULL;
        return DDERR_NOTFOUND;
    }
    found->AddRef();
    *surface = found;
    return DD_OK;
}

/**
 * @brief Returns pixel format of the surface.
 */
STDMETHODIMP DirectDrawSurfaceEmu::GetPixelFormat(LPDDPIXELFORMAT format)
{
    LOG_METHOD();
    CHECK_STRUCTURE(format, DDPIXELFORMAT);

    *format = desc.ddpfPixelFormat;
    return DD_OK;
}

/**
 * @brief Returns descriptor of this surface.
 */
STDMETHODIMP DirectDrawSurfaceEmu::GetSurfaceDesc(LPDDSURFACEDESC desc)
{
    LOG_METHOD();
    CHECK_STRUCTURE(desc, DDSURFACEDESC);

    *desc = this->desc;
    return DD_OK;
}

/**
 * @brief Lock specified part of the surface.
 */
STDMETHODIMP DirectDrawSurfaceEmu::Lock(LPRECT rect, LPDDSURFACEDESC desc, DWORD flags, HANDLE handle)
{
    LOG_METHOD();
    CHECK_STRUCTURE(desc, DDSURFACEDESC);
    HWEVENT(hw_layer, L"Lock");

    // TODO: Check that we are not locked recursively with different parameters. KA does not do that
    // so not emulated.

    // We need to detect if we are called from cloaking field read or for something else.
    // All KA locking in 3d rendering shares single code path without use of read only flags
    // so we need to detect that in different way. Find the return address.

    DWORD caller;
    __asm {
        mov eax, [ebp + 4]
        mov caller, eax
    }

    // If that is one known value, look-up next return address in the chain.
    // The game is compiled with the "Omit frame pointers" optimizations so we need
    // to access fixed offset for this specific caller.

    if (caller == 0x410113){
        DWORD caller2;
        __asm {
            mov eax, [ebp + 160]
            mov caller2, eax
        }

        // If it is lock for purpose of cloaking field creation, set the read only
        // flag. This will prevent the composition hack from activating and also prevent
        // texture upload on the unlock.

        if (caller2 == 0x00472d11) {
            flags |= DDLOCK_READONLY;
            logKA(MSG_VERBOSE, 1, "Cloaking field read");
            hw_layer.marker(L"Cloaking field read");
        }

        // The same for screenshots.

        else if (caller2 == 0x004DA47C) {
            flags |= DDLOCK_READONLY;
            logKA(MSG_VERBOSE, 1, "Screenshot read");
            hw_layer.marker(L"Screenshot read");
        }
    }

    // logKA info.

    if (rect) {
        logKA(MSG_VERBOSE, 1, "Lock %dx%d-%dx%d %u %x %08x", rect->left, rect->top, rect->right, rect->bottom, flags, handle, caller);
    }
    else {
        logKA(MSG_VERBOSE, 1, "Lock %u %x %08x", flags, handle, caller);
    }

    // Update the lock counter.

    lock_count++;

    // Store informations about the surface.

    desc->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_LPSURFACE;
    desc->dwWidth = this->desc.dwWidth;
    desc->dwHeight = this->desc.dwHeight;
    desc->lPitch = this->desc.lPitch;
    desc->ddpfPixelFormat = this->desc.ddpfPixelFormat;

    // Calculate location of the memory.

    const size_t x_offset = rect ? (rect->left * this->desc.ddpfPixelFormat.dwRGBBitCount / 8) : 0;
    const size_t y_offset = rect ? (rect->top * this->desc.lPitch) : 0;
    desc->lpSurface = (rect == NULL) ? memory : (static_cast<char *>(memory) + x_offset + y_offset);

    // For backbuffer ensure that any queued geometry is present on the screen.

    if (this->desc.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER) {
        flush_geometry();
    }

    if (! is_inside_sfad3d()) {

        // Hacks for KA.

        if ((this->desc.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER) && (rect == NULL) && (master != MASTER_MEMORY) && (flags == 2049)) {
            EmulationInfo &info = get_emulation_info();
            logKA(MSG_VERBOSE, 1, "Emulation state %u", info.emulation_state);

            // Quality improvement hack. The KA locks the surface so it can draw HUD or text which
            // results in 24bit->16bit->24bit conversion. To prevent this action from damaging
            // the 24 bit framebuffer, return black content and use the composition API on unlock.

            if (
                (info.emulation_state == EmulationInfo::EMULATION_STATE_3D_FLIP) ||
                (info.emulation_state == EmulationInfo::EMULATION_STATE_3D_SCENE_POINT_GEOMETRY_DRAWN) ||
                (info.emulation_state == EmulationInfo::EMULATION_STATE_3D_SCENE_TRIANGLE_GEOMETRY_DRAWN)
            ) {
                // If the memory already contains a composition content, we will simply
                // add to it. Otherwise we create a 'transparent' black color.

                if ((master != MASTER_COMPOSITION) && (master != MASTER_COMPOSITION_NONKEY)) {
                    assert(KA_COMPOSITION_KEY_MEMORY == 0);
                    memset(memory, 0, desc->lPitch * desc->dwHeight);
                }

                active_lock_hack = LOCK_HACK_COMPOSITION;
                logKA(MSG_VERBOSE, 1, "Composition hack activated");
                hw_layer.marker(L"Composition hack activated");
                return DD_OK;
            }

            // Starfield hack. The game begins scene, clears the buffer and uses CPU to draw background dots into
            // the buffer. Emulate this in the same way as with the hud to avoid format conversion on the CPU.
            // We add the first draw (the skybox) with overridden mode. If the CPU starfield is disabled, we
            // leave random content in the surface as it will be not uploaded.

            if (info.emulation_state == EmulationInfo::EMULATION_STATE_3D_SCENE) {
                if (is_cpu_starfield_enabled() && (master != MASTER_COMPOSITION) && (master != MASTER_COMPOSITION_NONKEY)) {
                    assert(KA_COMPOSITION_KEY_MEMORY == 0);
                    memset(memory, 0, desc->lPitch * desc->dwHeight);
                }
                active_lock_hack = LOCK_HACK_STARFIELD;
                logKA(MSG_VERBOSE, 1, "Starfield hack activated");
                hw_layer.marker(L"Starfield hack activated");
                return DD_OK;
            }
        }

    }
    else {

        // Hacks for SFA.

        if ((this->desc.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER) && (rect == NULL) && (master != MASTER_MEMORY) && (master != MASTER_NONE) && (flags == 1)) {

            // Quality improvement hack. The SFA locks the surface so it can draw HUD or text which
            // results in 24bit->16bit->24bit conversion. To prevent this action from damaging
            // the 24 bit framebuffer, return content filled with key color and use the composition
            // API on unlock.

            if ((master != MASTER_COMPOSITION) && (master != MASTER_COMPOSITION_NONKEY)) {
                assert((desc->lPitch % 4) == 0);
                const DWORD clear_dword = ((SFA_COMPOSITION_KEY_MEMORY << 16) | (SFA_COMPOSITION_KEY_MEMORY));
                const size_t dword_count = desc->lPitch * desc->dwHeight / 4;
                const void * const memory_to_set = memory;
                __asm {
                    mov eax, clear_dword
                    mov edi, memory_to_set
                    mov ecx, dword_count
                    rep stosd
                }
            }

            active_lock_hack = LOCK_HACK_COMPOSITION;
            logKA(MSG_VERBOSE, 1, "Composition hack activated");
            hw_layer.marker(L"Composition hack activated");
            return DD_OK;
        }
    }

    // Ensure that the memory contains the latest content.

    synchronize_memory();

    // Unless the surface is locked for read, consider the HW
    // copy to be stalled. The Klingon Academy reads the depth
    // surface for visibility queries without proper flag, assume
    // read-only behavior for them as well.

    if (((flags & DDLOCK_READONLY) == 0) && ((this->desc.ddsCaps.dwCaps & DDSCAPS_ZBUFFER) == 0)) {
        logKA(MSG_VERBOSE, 1, "Memory copy is now master");
        master = MASTER_MEMORY;
    }

    return DD_OK;
}

/**
 * @brief Unlocks the surface.
 */
STDMETHODIMP DirectDrawSurfaceEmu::Unlock(LPVOID addr)
{
    LOG_METHOD();
    CHECK_NOT_NULL(addr);
    HWEVENT(hw_layer, L"Unlock");

    // It appears that the Klingon Academy sometimes unlocks surface more than once.

    if (lock_count == 0) {
        return DDERR_NOTLOCKED;
    }
    lock_count--;

    // See Lock().

    if (! is_inside_sfad3d()) {

        // Hacks for KA.

        if (active_lock_hack == LOCK_HACK_COMPOSITION) {
            assert((master == MASTER_HW) || (master == MASTER_SYNCHRONIZED) || (master == MASTER_COMPOSITION) || (master == MASTER_COMPOSITION_NONKEY));
            assert(this->desc.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER);
            assert(hw_surface);

            active_lock_hack = LOCK_HACK_NONE;

            // The lock after end of the scene and before flip is used to draw
            // the UI so it is almost certainly non-zero.

            EmulationInfo &info = get_emulation_info();
            if (info.emulation_state == EmulationInfo::EMULATION_STATE_3D_FLIP) {
                master = MASTER_COMPOSITION_NONKEY;
            }

            // Otherwise keep the non-zero state if it was set previously.

            else if (master != MASTER_COMPOSITION_NONKEY) {
                master = MASTER_COMPOSITION;
            }
            return DD_OK;
        }

        if (active_lock_hack == LOCK_HACK_STARFIELD) {
            assert((master == MASTER_HW) || (master == MASTER_SYNCHRONIZED) || (master == MASTER_COMPOSITION) || (master == MASTER_COMPOSITION_NONKEY));
            assert(this->desc.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER);
            assert(hw_surface);

            active_lock_hack = LOCK_HACK_NONE;

            // The starfield always contains some content.

            master = is_cpu_starfield_enabled() ? MASTER_COMPOSITION_NONKEY : MASTER_HW;
            return DD_OK;
        }

    }
    else {

        // Hacks for SFA.

        if (active_lock_hack == LOCK_HACK_COMPOSITION) {
            assert((master == MASTER_HW) || (master == MASTER_SYNCHRONIZED) || (master == MASTER_COMPOSITION) || (master == MASTER_COMPOSITION_NONKEY));
            assert(this->desc.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER);
            assert(hw_surface);

            active_lock_hack = LOCK_HACK_NONE;
            master = MASTER_COMPOSITION_NONKEY;
            return DD_OK;
        }
    }

    update_presentation_emulation();
    return DD_OK;
}

/**
 * @brief Creates execute buffer.
 */
STDMETHODIMP DirectDrawSurfaceEmu::CreateExecuteBuffer(LPD3DEXECUTEBUFFERDESC buffer_desc,LPDIRECT3DEXECUTEBUFFER * buffer, IUnknown * outer)
{
    LOG_METHOD();
    CHECK_STRUCTURE(buffer_desc, D3DEXECUTEBUFFERDESC);
    CHECK_NOT_NULL(buffer);
    CHECK_NULL(outer);

    log_structure(MSG_VERBOSE, 1, *buffer_desc);

    // Check structure we got.

    if ((buffer_desc->dwFlags & D3DDEB_BUFSIZE) == 0) {
        logKA(MSG_ERROR, 0, "CreateExecuteBuffer:Buffer size not specified.");
        return DDERR_INVALIDPARAMS;
    }

    // Prepare the buffer.

    *buffer = new Direct3DExecuteBufferEmu(hw_layer, buffer_desc->dwBufferSize);
    logKA(MSG_VERBOSE, 1, "Created execute buffer %08x", *buffer);

    buffer_desc->dwFlags |= D3DDEB_CAPS;
    buffer_desc->dwCaps = D3DDEBCAPS_VIDEOMEMORY;
    return DD_OK;
}

/**
 * @brief Executes the execute buffer.
 */
STDMETHODIMP DirectDrawSurfaceEmu::Execute(LPDIRECT3DEXECUTEBUFFER buffer,LPDIRECT3DVIEWPORT viewport,DWORD flags)
{
    LOG_METHOD();
    CHECK_NOT_NULL(buffer);
    CHECK_NOT_NULL(viewport);
    HWEVENT(hw_layer, L"Execute");

    static_cast<Direct3DExecuteBufferEmu *>(buffer)->execute(*this, viewport, flags);
    return DD_OK;
}

/**
 * @brief Adds viewport to list of registered viewports.
 */
STDMETHODIMP DirectDrawSurfaceEmu::AddViewport(LPDIRECT3DVIEWPORT viewport)
{
    LOG_METHOD();
    CHECK_NOT_NULL(viewport);

    viewport->AddRef();
    viewports.push_back(viewport);
    return DD_OK;
}

/**
 * @brief Removes viewport from list of registered viewports.
 */
STDMETHODIMP DirectDrawSurfaceEmu::DeleteViewport(LPDIRECT3DVIEWPORT viewport)
{
    LOG_METHOD();
    CHECK_NOT_NULL(viewport);

    std::deque<IDirect3DViewport *>::iterator it;
    for (it = viewports.begin(); it != viewports.end(); ++it) {
        if (*it == viewport) {
            viewports.erase(it);
            viewport->Release();
            return DD_OK;
        }
    }
    return DDERR_INVALIDPARAMS;
}

/**
 * @brief Enumerates supported texture formats.
 */
STDMETHODIMP DirectDrawSurfaceEmu::EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK callback, LPVOID user_data)
{
    LOG_METHOD();
    CHECK_NOT_NULL(callback);

    DDSURFACEDESC desc = { 0 };
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_PIXELFORMAT;
    desc.ddpfPixelFormat.dwSize = sizeof(desc.ddpfPixelFormat);

    for (size_t i = HWFORMAT_R5G6B5 ; i <= HWFORMAT_R4G4B4A4 ; ++i) {
        desc.ddpfPixelFormat = texture_formats[i];
        if (callback(&desc, user_data) != DDENUMRET_OK) {
            return DD_OK;
        }
    }

    return DD_OK;
}

/**
 * @brief Returns handle for this texture for specified d3d device.
 */
STDMETHODIMP DirectDrawSurfaceEmu::GetHandle(LPDIRECT3DDEVICE device, LPD3DTEXTUREHANDLE handle)
{
    LOG_METHOD();
    CHECK_NOT_NULL(device);
    CHECK_NOT_NULL(handle);

    // HACK: Pointer to us is our handle. This works only for 32 bit process.
    // The api we are emulating is ancient so it was never used by
    // any 64 bit process.

    *handle = reinterpret_cast<DWORD>(this);
    return DD_OK;
}

/**
 * @brief Called before any 3d rendering.
 */
STDMETHODIMP DirectDrawSurfaceEmu::BeginScene()
{
    LOG_METHOD();

    // HACK: The KA loading screen uses begin/endscene without flip. Because
    // we are waiting uncoditionally for the Flip, doing this prevents
    // any updates until the load completes so the warp screen is invisible.
    // If we detect two BeginScene without interleaving Flip, we assume that this
    // problem is happening and force update of the primary surface.

    EmulationInfo &info = get_emulation_info();
    if (info.emulation_state == EmulationInfo::EMULATION_STATE_3D_FLIP) {
        DirectDrawSurfaceEmu * const front_buffer = find_front_buffer();
        if (front_buffer) {
            info.emulation_state = EmulationInfo::EMULATION_STATE_WAITING_FOR_TIME;
            front_buffer->update_presentation_emulation();
        }
    }

    // Normal operation.

    hw_layer.start_event(L"scene");
    assert(find_back_buffer() == this);
    hw_layer.begin_scene();

    // Bind the correct render target.

    DirectDrawSurfaceEmu * const back = find_back_buffer();
    DirectDrawSurfaceEmu * const depth = find_depth_buffer();
    hw_layer.set_render_target(
        (back ? back->get_hw_surface(true) : NULL),
        (depth ? depth->get_hw_surface(true) : NULL)
    );

    scene_active = true;

    // From now on we will be using flip as presentation
    // event.

    info.emulation_state = EmulationInfo::EMULATION_STATE_3D_SCENE;
    return DD_OK;
}

/**
 * @brief Called after all 3d rendering.
 */
STDMETHODIMP DirectDrawSurfaceEmu::EndScene()
{
    LOG_METHOD();
    assert(find_back_buffer() == this);
    hw_layer.end_scene();
    hw_layer.set_render_target(NULL, NULL);
    scene_active = false;

    // We will wait for the flip which will present the scene.

    EmulationInfo &info = get_emulation_info();
    info.emulation_state = EmulationInfo::EMULATION_STATE_3D_FLIP;
    hw_layer.end_event();
    return DD_OK;
}

/**
 * @brief Loads the texture.
 */
STDMETHODIMP DirectDrawSurfaceEmu::Load(LPDIRECT3DTEXTURE texture)
{
    LOG_METHOD();
    CHECK_NOT_NULL(texture);
    HWEVENT(hw_layer, L"Load");
    logKA(MSG_VERBOSE, 1, "From %08x", texture);

    // The memory is already allocated.

    DirectDrawSurfaceEmu * const impl = static_cast<DirectDrawSurfaceEmu *>(texture);
    assert(desc.dwWidth == impl->desc.dwWidth);
    assert(desc.dwHeight == impl->desc.dwHeight);
    assert(memcmp(&desc.ddpfPixelFormat, &impl->desc.ddpfPixelFormat, sizeof(desc.ddpfPixelFormat)) == 0);

    // Copy the data.

    const size_t memory_size = desc.dwHeight * desc.lPitch;
    memcpy(memory, impl->memory, memory_size);
    master = MASTER_MEMORY;

    update_presentation_emulation();
    return DD_OK;
}

/**
 * @brief Unloads the texture.
 */
STDMETHODIMP DirectDrawSurfaceEmu::Unload()
{
    LOG_METHOD();
    return DD_OK;
}

} // namespace emu

// EOF //
