#include "dx9_hw_layer.h"
#include "../../helpers/log.h"
#include "../../helpers/config.h"
#include <stdlib.h>
#include <assert.h>

namespace emu {

namespace {

/**
 * @brief Format to use for backbuffer.
 */
const D3DFORMAT BACKBUFFER_FORMAT = D3DFMT_X8R8G8B8;

/**
 * @brief FVF for use with the game generated geometry.
 */
const DWORD STANDARD_FVF_NORMAL = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0);

/**
 * @brief FVF for use with the game generated geometry in 3D Vision mode.
 *
 * The vertex format really is XYZRHW however we need
 * vertex shader to run so it can be 3d-ized.
 */
const DWORD STANDARD_FVF_VISION = D3DFVF_XYZW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0);

/**
 * @brief Maximal number of vertices in single execute buffer.
 */
const size_t MAXIMAL_VERTEX_COUNT = 200000;

/**
 * @brief Maximal number of indices in single draw.
 */
const size_t MAXIMAL_INDEX_COUNT = 100000;

/**
 * @brief Size of vertex buffer we are using. Should be sufficient for any single execute buffer.
 */
const size_t VERTEX_BUFFER_SIZE = sizeof(TLVertex) * MAXIMAL_VERTEX_COUNT;

/**
 * @brief Size of index buffer we are using. Should be sufficient for any single draw.
 */
const size_t INDEX_BUFFER_SIZE = sizeof(unsigned short) * MAXIMAL_INDEX_COUNT;


#include "shaders/fragment_present.h"
#include "shaders/vertex_present.h"

#include "shaders/fragment_compose.h"
#include "shaders/vertex_compose.h"

#include "shaders/fragment_compose_non_black_key.h"
#include "shaders/vertex_compose_non_black_key.h"

#include "shaders/fragment_copy.h"
#include "shaders/vertex_copy.h"

#include "shaders/fragment_base.h"
#include "shaders/fragment_vfog.h"
#include "shaders/fragment_tfog.h"

#include "shaders/fragment_txt.h"
#include "shaders/fragment_txt_vfog.h"
#include "shaders/fragment_txt_tfog.h"

#include "shaders/fragment_txt_mod.h"
#include "shaders/fragment_txt_mod_vfog.h"
#include "shaders/fragment_txt_mod_tfog.h"

#include "shaders/vertex_base.h"
#include "shaders/vertex_vfog.h"
#include "shaders/vertex_tfog.h"

#include "shaders/vertex_txt.h"
#include "shaders/vertex_txt_vfog.h"
#include "shaders/vertex_txt_tfog.h"

#include "shaders/vertex_txt_mod.h"
#include "shaders/vertex_txt_mod_vfog.h"
#include "shaders/vertex_txt_mod_tfog.h"

/**
 * @brief Source for vertex shaders to use in normal mode.
 *
 * The standard geometry is using fixed function pipeline on XYZRHW vertices.
 */
const BYTE * const vertex_shader_sources_normal[DX9_SHADER_VARIANT_COUNT] = {
    vs_shader_present,
    vs_shader_compose,
    vs_shader_compose_non_black_key,
    vs_shader_copy,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

/**
 * @brief Source for vertex shaders to use in normal mode.
 *
 * The standard geometry is using fixed function pipeline on XYZRHW vertices.
 */
const BYTE * const vertex_shader_sources_vision[DX9_SHADER_VARIANT_COUNT] = {
    vs_shader_present,
    vs_shader_compose,
    vs_shader_compose_non_black_key,
    vs_shader_copy,
    vs_shader_base,
    vs_shader_vfog,
    vs_shader_tfog,
    vs_shader_txt,
    vs_shader_txt_vfog,
    vs_shader_txt_tfog,
    vs_shader_txt_mod,
    vs_shader_txt_mod_vfog,
    vs_shader_txt_mod_tfog,
};

/**
 * @brief Source for vertex shaders to use in 3D vision mode.
 */
const BYTE * const fragment_shader_sources[DX9_SHADER_VARIANT_COUNT] = {
    fs_shader_present,
    fs_shader_compose,
    fs_shader_compose_non_black_key,
    fs_shader_copy,
    fs_shader_base,
    fs_shader_vfog,
    fs_shader_tfog,
    fs_shader_txt,
    fs_shader_txt_vfog,
    fs_shader_txt_tfog,
    fs_shader_txt_mod,
    fs_shader_txt_mod_vfog,
    fs_shader_txt_mod_tfog,
};

const size_t SHADER_PRESENT = 0;
const size_t SHADER_COMPOSE = 1;
const size_t SHADER_COMPOSE_NON_BLACK_KEY = 2;
const size_t SHADER_COPY = 3;
const size_t SHADER_GAME_BASE = 4;

/**
 * @brief Returns index of shader for specified combination of effects.
 */
int get_shader_index(const bool texture, const TextureBlend txt_blend, const Fog fog)
{
    if (! texture) {
        return SHADER_GAME_BASE + fog;
    }
    else if (txt_blend == TEXTURE_BLEND_MODULATE) {
        return SHADER_GAME_BASE + SIZE_OF_FOG + fog;
    }
    else {
        return SHADER_GAME_BASE + (SIZE_OF_FOG * 2) + fog;
    }
}

size_t log2_rounded_up(const size_t value)
{
    size_t result = 0;
    for (; value > (1u << result); ++result) {
    }
    return result;
}

/**
 * @brief Return cache slot corresponding to specified combination of
 * parmeters or 0 if the combination is not cacheable.
 */
size_t get_cache_slot(const size_t width, const size_t height, const HWFormat format)
{
    // Component for the format.

    if (format < HWFORMAT_R5G6B5) {
        return 0;
    }
    const unsigned format_cache_index = (format - HWFORMAT_R5G6B5);
    if (format_cache_index >= SURFACE_CACHE_FORMAT_SLOTS) {
        return 0;
    }

    // Component for the width.

    if (width > MAX_CACHED_SURFACE_SIZE) {
        return 0;
    }
    const size_t width_cache_index = log2_rounded_up(width);
    if ((1u << width_cache_index) != width) {
        return 0;
    }

    // Component for the height.

    if (height > MAX_CACHED_SURFACE_SIZE) {
        return 0;
    }
    const size_t height_cache_index = log2_rounded_up(height);
    if ((1u << height_cache_index) != height) {
        return 0;
    }

    // Pack everything into the index.

    assert(format_cache_index < SURFACE_CACHE_FORMAT_SLOTS);
    assert(width_cache_index < SURFACE_CACHE_SIZE_SLOTS);
    assert(height_cache_index < SURFACE_CACHE_SIZE_SLOTS);
    return ((height_cache_index * SURFACE_CACHE_SIZE_SLOTS) + width_cache_index) * SURFACE_CACHE_FORMAT_SLOTS + format_cache_index + 1;
}

inline HRESULT log_d3d_error_helper(const int line, const char * const action, const HRESULT value)
{
    if (FAILED(value)) {
        logKA(MSG_ERROR, 0, "HW:Operation failed %d:'%s':%08x", line, action, value);
    }
    return value;
}

#define log_error(expression) log_d3d_error_helper(__LINE__, #expression, (expression))

/**
 * @brief Appends data into vertex or index buffer, discarding the old
 * content if necessary.
 *
 * Returns index of first vertex in the buffer.
 */
template<typename BufferType, typename EntryType>
static size_t fill_buffer(BufferType &buffer, size_t & free_index, const size_t entry_limit, const EntryType * const data, const size_t count)
{
    if (count == 0) {
        return free_index;
    }

    assert(count <= entry_limit);
    const size_t data_size = sizeof(EntryType) * count;

    // Reset the position if we do not fit into the free space.

    if ((free_index + count) > entry_limit) {
        free_index = 0;
    }

    // Discard old content if we are at start of the buffer.

    const DWORD lock_flags = (free_index == 0) ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;

    // Lock it.

    const size_t start_index = free_index;
    const size_t start_offset = sizeof(EntryType) * free_index;
    free_index += count;

    void * buffer_data = NULL;
    if (FAILED(log_error(buffer.Lock(start_offset, data_size, &buffer_data, lock_flags)))) {
        return start_index;
    }

    // Fill.

    memcpy(buffer_data, data, data_size);

    // Unlock.

    log_error(buffer.Unlock());

    return start_index;
}

/**
 * @brief Creates D3D event for lifetime of this object.
 */
class D3DEventGuard {

    public:
        D3DEventGuard(const wchar_t * const name) { D3DPERF_BeginEvent(0, name); }
        ~D3DEventGuard() { D3DPERF_EndEvent(); }
};

#ifdef HW_EVENTS
#define D3DEVENT(name) D3DEventGuard CONCAT(guard,__LINE__)((name));
#else
#define D3DEVENT(name)
#endif

} // anonymous namespace

DX9HWLayer::HWSurfaceInfo::HWSurfaceInfo()
    : width(0)
    , height(0)
    , mono_height(0)
    , stride(0)
    , format(HWFORMAT_NONE)
    , render_target(false)
    , dx_format(D3DFMT_UNKNOWN)
    , texture()
    , surface_0()
    , transfer_texture()
    , transfer_surface_0()
    , read_16b_texture_rt()
    , read_16b_texture()
    , read_16b_rt_surface_0()
    , read_16b_surface_0()
    , composition_texture()
    , msaa_render_target()
    , msaa_sync(MSAA_SYNC_TEXTURE)
    , cache_slot(0)
    , next_in_cache(NULL)
{
}

DX9HWLayer::HWState::HWState()
{
    reset();
}

DX9HWLayer::HWState::HWState(const DX9HWLayer::HWState &other)
    : depth_test(other.depth_test)
    , alpha_test(other.alpha_test)
    , alpha_blend(other.alpha_blend)
    , fog_mode(other.fog_mode)
    , fog_color(other.fog_color)
    , flat(other.flat)
    , texture_blend(other.texture_blend)
    , texture(other.texture)
    , color_info(other.color_info)
    , depth_info(other.depth_info)
{
}

void DX9HWLayer::HWState::reset(void)
{
    depth_test = DEPTH_TEST_NONE;
    alpha_test = ALPHA_TEST_NONE;
    alpha_blend = BLEND_NONE;
    fog_mode = FOG_NONE;
    fog_color = 0;
    flat = false;
    texture_blend = TEXTURE_BLEND_MODULATE;
    texture = NULL;
    color_info = NULL;
    depth_info = NULL;
}

DX9HWLayer::DX9HWLayer()
    : width(0)
    , height(0)
    , direct3d()
    , direct3d_ex()
    , vision_3d(false)
    , slow_zbuffer_readback(false)
    , max_anisotropy(1)
    , multisample_type(D3DMULTISAMPLE_NONE)
    , multisample_quality(0)
    , device()
    , device_ex()
    , default_color()
    , default_depth()
    , vertex_buffer()
    , vertex_data_start_index(0)
    , vertex_buffer_free_index(0)
    , index_buffer()
    , index_buffer_free_index(0)
    , state()
    , active_combination(-1)
    , scene_active(false)
{
    memset(cache, 0, sizeof(cache));
}

DX9HWLayer::~DX9HWLayer()
{
    deinitialize();
}

/**
 * @brief Returns list of available display modes.
 */
bool DX9HWLayer::get_display_modes(DisplayModeList &modes)
{
    logKA(MSG_INFORM, 0, "HW:Enumerating display modes");
    modes.clear();

    // Create the query interface if necessary.

    CComPtr<IDirect3D9> d3d = direct3d;
    if (! d3d) {
        d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
        if (! d3d) {
            logKA(MSG_ERROR, 0, "HW:Unable to create D3D9");
            return false;
        }
    }

    // Enumerate all modes.

    const size_t mode_count = d3d->GetAdapterModeCount(D3DADAPTER_DEFAULT, BACKBUFFER_FORMAT);
    for (size_t i = 0; i < mode_count; ++i) {
        D3DDISPLAYMODE mode;
        if (FAILED(d3d->EnumAdapterModes(D3DADAPTER_DEFAULT, BACKBUFFER_FORMAT, i, &mode))) {
            break;
        }
        if (mode.Format != BACKBUFFER_FORMAT) {
            continue;
        }
        modes.push_back(DisplayMode(mode.Width, mode.Height, mode.RefreshRate));
    }
    return true;
}

/**
 * @brief Initializes the device.
 */
bool DX9HWLayer::initialize(const HWND window, const size_t width, const size_t height)
{
    logKA(MSG_INFORM, 0, "HW:Initializing DX9 HW %ux%u", width, height);

    // Try to create the extended interface.

    if (! is_option_enabled("D3DEMU_NO_VISTA")) {
        HMODULE d3dlib = NULL;
        d3dlib = LoadLibrary(L"d3d9.dll");
        if (d3dlib != NULL) {
            typedef HRESULT (WINAPI *LPDIRECT3DCREATE9EX)(UINT, IDirect3D9Ex **);
            const LPDIRECT3DCREATE9EX Direct3DCreate9ExPtr = reinterpret_cast<LPDIRECT3DCREATE9EX>(GetProcAddress(d3dlib, "Direct3DCreate9Ex"));
            if (Direct3DCreate9ExPtr) {
                if (FAILED(log_error(Direct3DCreate9ExPtr(D3D_SDK_VERSION, &direct3d_ex)))) {
                    direct3d_ex = NULL;
                }
                else {
                    logKA(MSG_INFORM, 0, "HW:Created extended D3D interface. Alt+Tab is supported - use D3DEMU_NO_VISTA to disable creation of this interface");
                    direct3d = direct3d_ex;
                }
            }
            FreeLibrary(d3dlib);
        }
    }

    // The old interface if the new one is not available.

    if (! direct3d) {
        direct3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));
        if (! direct3d) {
            logKA(MSG_ERROR, 0, "HW:Unable to create D3D9");
            return false;
        }
    }

    // TODO: Some capability checks to detect unsupported HW.

    // Initialize the device.

    D3DPRESENT_PARAMETERS parameters;
    memset(&parameters, 0, sizeof(parameters));
    parameters.BackBufferWidth = width;
    parameters.BackBufferHeight = height;
    parameters.BackBufferFormat = BACKBUFFER_FORMAT;
    parameters.BackBufferCount = 1;
    parameters.MultiSampleType = D3DMULTISAMPLE_NONE;
    parameters.MultiSampleQuality = 0;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow = window;
    parameters.Windowed = FALSE;
    parameters.EnableAutoDepthStencil = FALSE;
    parameters.Flags = 0;
    parameters.FullScreen_RefreshRateInHz = 60;
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    const bool sw_mixing = is_option_enabled("D3DEMU_NO_HW_PROCESSING");
    if (sw_mixing) {
        logKA(MSG_INFORM, 0, "HW:Using CPU based vertex processing");
    }
    else {
        logKA(MSG_INFORM, 0, "HW:Using GPU based vertex processing - use D3DEMU_NO_HW_PROCESSING to disable it");
    }

    const DWORD create_flags =
        (sw_mixing ? D3DCREATE_SOFTWARE_VERTEXPROCESSING : (D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE)) |
        (direct3d_ex ? D3DCREATE_DISABLE_PSGP_THREADING : 0) | // Just to be sure.
        D3DCREATE_FPU_PRESERVE // Klingon Academy uses float->int conversion which depends on this.
    ;

    size_t adapter = D3DADAPTER_DEFAULT;
    D3DDEVTYPE device_type = D3DDEVTYPE_HAL;

    // Try to find the PerfHUD so we can register to it.

    for (size_t i = 0; i < direct3d->GetAdapterCount(); ++i) {
        D3DADAPTER_IDENTIFIER9 identifier;
        if (FAILED(direct3d->GetAdapterIdentifier(i, 0, &identifier))) {
            continue;
        }
        if (strstr(identifier.Description, "PerfHUD") != NULL) {
            adapter = i;
            device_type = D3DDEVTYPE_REF;
            logKA(MSG_INFORM, 0, "HW:PerfHUD detected");
            break;
        }
    }

    // Call proper creator.

    HRESULT result;
    if (direct3d_ex) {
        D3DDISPLAYMODEEX mode_ex;
        memset(&mode_ex, 0, sizeof(mode_ex));
        mode_ex.Size = sizeof(D3DDISPLAYMODEEX);
        mode_ex.Width = parameters.BackBufferWidth;
        mode_ex.Height = parameters.BackBufferHeight;
        mode_ex.RefreshRate = parameters.FullScreen_RefreshRateInHz;
        mode_ex.Format = parameters.BackBufferFormat;
        mode_ex.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;

        result = direct3d_ex->CreateDeviceEx(adapter, device_type, window, create_flags, &parameters, &mode_ex, &device_ex);
        device = device_ex;
    }
    else {
        result = direct3d->CreateDevice(adapter, device_type, window, create_flags, &parameters, &device);
    }

    if (FAILED(result)) {
        logKA(MSG_ERROR, 0, "HW:Unable to create device %08x", result);
        direct3d_ex = NULL;
        direct3d = NULL;
        return false;
    }

    // Detect various capabilities.

    detect_slow_z_readback(adapter);
    detect_anisotropy(adapter, device_type);
    detect_msaa(adapter, device_type);

    // Enable the 3d vision support if requested. It is not enabled by default
    // as it results in bigger texture which needs to be transfered across
    // the buss.

    vision_3d = is_option_enabled("D3DEMU_3D_VISION");

    // Create shaders.

    const BYTE * const * const vertex_shader_sources = vision_3d ? vertex_shader_sources_vision : vertex_shader_sources_normal;
    for (size_t i = 0; i < DX9_SHADER_VARIANT_COUNT; ++i) {
        if (! vertex_shader_sources[i]) {
            continue;
        }
        const HRESULT result = device->CreateVertexShader(reinterpret_cast<const DWORD *>(vertex_shader_sources[i]), &vertex_shaders[i]);
        if (FAILED(result)) {
            logKA(MSG_ERROR, 0, "HW:Unable to create vertex shader variant %u: %08x", i, result);
            device_ex = NULL;
            device = NULL;
            direct3d_ex = NULL;
            direct3d = NULL;
            return false;
        }
    }

    for (size_t i = 0; i < DX9_SHADER_VARIANT_COUNT; ++i) {
        assert(fragment_shader_sources[i]);
        const HRESULT result = device->CreatePixelShader(reinterpret_cast<const DWORD *>(fragment_shader_sources[i]), &fragment_shaders[i]);
        if (FAILED(result)) {
            logKA(MSG_ERROR, 0, "HW:Unable to create fragment shader variant %u: %08x", i, result);
            device_ex = NULL;
            device = NULL;
            direct3d_ex = NULL;
            direct3d = NULL;
            return false;
        }
    }

    // Init buffers.

    vertex_buffer_free_index = 0;
    index_buffer_free_index = 0;

    if (! is_option_enabled("D3DEMU_NO_BUFFERS")) {
        logKA(MSG_INFORM, 0, "HW:Using vertex and index buffers - use D3DEMU_NO_BUFFERS to disable them.");
        result = device->CreateVertexBuffer(VERTEX_BUFFER_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, vision_3d ? STANDARD_FVF_VISION : STANDARD_FVF_NORMAL, D3DPOOL_DEFAULT, &vertex_buffer, NULL);
        if (FAILED(result)) {
            logKA(MSG_ERROR, 0, "HW:Unable to create vertex buffer: %08x", result);
            device_ex = NULL;
            device = NULL;
            direct3d_ex = NULL;
            direct3d = NULL;
            return false;
        }

        result = device->CreateIndexBuffer(INDEX_BUFFER_SIZE, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &index_buffer, NULL);
        if (FAILED(result)) {
            logKA(MSG_ERROR, 0, "HW:Unable to create vertex buffer: %08x", result);
            device_ex = NULL;
            device = NULL;
            direct3d_ex = NULL;
            direct3d = NULL;
            return false;
        }
    }

    // Find the default render targets. The default swap chain
    // does not have the depth buffer.

    log_error(device->GetRenderTarget(0, &default_color));
    default_depth = NULL;

    // Configure the device.

    set_default_states();

    this->width = width;
    this->height = height;
    return true;
}

/**
 * @brief Detects slow readback of the depth buffer and update slow_zbuffer_readback
 * appropriatelly.
 */
void DX9HWLayer::detect_slow_z_readback(const size_t adapter)
{
    // Detect z-buffer readback capabilities. Be optimistic.

    slow_zbuffer_readback = false;

    D3DADAPTER_IDENTIFIER9 adapter_id;
    memset(&adapter_id, 0, sizeof(adapter_id));
    if (SUCCEEDED(direct3d->GetAdapterIdentifier(adapter, 0, &adapter_id))) {
        logKA(MSG_INFORM, 0, "HW:Vendor ID: 0x%X", adapter_id.VendorId);

        // ATI has slow readback.

        if (adapter_id.VendorId == 0x1002) {
            slow_zbuffer_readback = true;
            logKA(MSG_INFORM, 0, "HW:ATI detected, disabling emulation of depth buffer readback");
        }
    }
    else {

        // Assume worst.

        slow_zbuffer_readback = true;
    }

}

/**
 * @brief Detects anisotropy level to use and assigns it to max_anisotropy member.
 */
void DX9HWLayer::detect_anisotropy(const size_t adapter, const D3DDEVTYPE device_type)
{
    D3DCAPS9 caps;
    memset(&caps, 0, sizeof(caps));
    if (FAILED(direct3d->GetDeviceCaps(adapter, device_type, &caps))) {
        memset(&caps, 0, sizeof(caps));
    }

    // Detect level of anisotropic filtering.

    max_anisotropy = 1;
    const size_t value_override = get_anisotropy_level();
    if (value_override > 0) {
        max_anisotropy = 1 + value_override;
    }
    max_anisotropy = min(max_anisotropy, caps.MaxAnisotropy);
    max_anisotropy = max(max_anisotropy, 1);

    if (max_anisotropy > 1) {
        logKA(MSG_INFORM, 0, "HW:Using DX anisotropy: %u", max_anisotropy);
    }
    else {
        logKA(MSG_INFORM, 0, "HW:Anisotropic filtering disabled, define D3DEMU_ANISOTROPY with desired anisotropy level >= 1 or 'max' keyword to enable it");
    }
}

/**
 * @brief Detects msaa level to use and sets the multisample_* members.
 */
void DX9HWLayer::detect_msaa(const size_t adapter, const D3DDEVTYPE device_type)
{
    multisample_type = D3DMULTISAMPLE_NONE;
    multisample_quality = 0;

    // The MSAA is disabled unless overriden by user.

    const size_t user_specified_level = get_msaa_quality_level();
    if (user_specified_level == 0) {
        logKA(MSG_INFORM, 0, "HW:MSAA not enabled, define D3DEMU_MSAA_QUALITY with desired quality level >= 1 or 'max' keyword to enable it");
        return;
    }

    // Detect maximal possible quality.

    DWORD quality_count_color;
    DWORD quality_count_depth;
    if (
        SUCCEEDED(direct3d->CheckDeviceMultiSampleType(adapter, device_type, D3DFMT_A8R8G8B8, FALSE, D3DMULTISAMPLE_NONMASKABLE, &quality_count_color)) && (quality_count_color > 0) &&
        SUCCEEDED(direct3d->CheckDeviceMultiSampleType(adapter, device_type, D3DFMT_D24X8, FALSE, D3DMULTISAMPLE_NONMASKABLE, &quality_count_depth)) && (quality_count_depth > 0)
    ) {
        multisample_type = D3DMULTISAMPLE_NONMASKABLE;
        multisample_quality = min(min(quality_count_color, quality_count_depth), user_specified_level) - 1;
        logKA(MSG_INFORM, 0, "HW:Using DX MSAA quality: %u", multisample_quality);
    }
}

void DX9HWLayer::deinitialize(void)
{
    if (direct3d == NULL) {
        return;
    }

    logKA(MSG_INFORM, 0, "HW:Deinitializing DX9 emu");

    // Destroy surface cache. We do not care to keep the list
    // consistent after each operation.

    for (size_t i = 0; i < SURFACE_CACHE_SLOTS; ++i) {
        while (cache[i].head) {
            HWSurfaceInfo *const head = cache[i].head;
            cache[i].head = head->next_in_cache;
            delete head;
        }
        cache[i].tail = NULL;
    }

    // Free all shaders we have.

    activate_shader_combination(-1);
    for (size_t i = 0; i < DX9_SHADER_VARIANT_COUNT; ++i) {
        vertex_shaders[i] = NULL;
        fragment_shaders[i] = NULL;
    }

    // State cleanup.

    if (device) {
        HWState empty_state;
        apply_state(empty_state, true);
    }
    state.reset();

    // Object cleanup.

    vertex_buffer = NULL;
    index_buffer = NULL;
    default_depth = NULL;
    default_color = NULL;
    device_ex = NULL;
    device = NULL;
    direct3d_ex = NULL;
    direct3d = NULL;
    height = 0;
    width = 0;
}

void DX9HWLayer::begin_scene(void)
{
    D3DEVENT(L"begin_scene");
    logKA(MSG_VERBOSE, 0, "HW:begin_scene");
    log_error(device->BeginScene());
    scene_active = true;
}

void DX9HWLayer::end_scene(void)
{
    D3DEVENT(L"end_scene");
    scene_active = false;
    log_error(device->EndScene());
    logKA(MSG_VERBOSE, 0, "HW:end_scene");
}

HWSurfaceHandle DX9HWLayer::create_surface(const size_t width, const size_t height, const HWFormat format, const void * const memory, const bool render_target)
{
    D3DEVENT(L"create_surface");

    // Depth buffers are handled in special way.

    if (format == HWFORMAT_ZBUFFER) {
        return create_depth_surface(width, height);
    }

    // Try to find existing entry in the cache.

    const size_t cache_slot = ((! render_target) && is_surface_cache_enabled()) ? get_cache_slot(width, height, format) : 0;
    if ((cache_slot != 0) && cache[cache_slot].head) {

        // Remove entry from the cache.

        HWSurfaceInfo * const info = cache[cache_slot].head;
        cache[cache_slot].head = info->next_in_cache;
        info->next_in_cache = NULL;
        if (cache[cache_slot].head == NULL) {
            cache[cache_slot].tail = NULL;
        }

        // Check that we got surface with the expected parameters.

        assert(info->width == width);
        assert(info->height == height);
        assert(info->format == format);
        assert(info->render_target == render_target);

        // Upload data if we have any.
        // Note that render targets are not cached so there
        // is no need to manage synchronization.

        if (memory) {
            update_surface(info, memory);
        }
        return info;
    }

    const DWORD managed_usage = (direct3d_ex != NULL) ? D3DUSAGE_DYNAMIC : 0;
    const D3DPOOL managed_memory_pool = (direct3d_ex != NULL) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;

    // Prepare the parameters. We use 32 bit render target and
    // convert the color spaces during upload/read.

    DWORD usage;
    D3DPOOL pool;
    D3DFORMAT d3d_format;
    size_t mipmap_count;

    if (render_target) {

        // Currently only fixed format is supported by the conversion.

        assert(format == HWFORMAT_R5G6B5);
        usage = D3DUSAGE_RENDERTARGET;
        pool = D3DPOOL_DEFAULT;
        d3d_format = D3DFMT_A8R8G8B8;
        mipmap_count = 1;
    }
    else {
        usage = managed_usage;
        pool = managed_memory_pool;
        d3d_format = (format == HWFORMAT_R5G6B5) ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8;
        mipmap_count = 1;

        // Enable mipmap autogen if available.

        if (direct3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_AUTOGENMIPMAP, D3DRTYPE_TEXTURE, d3d_format) == D3D_OK) {
            usage |= D3DUSAGE_AUTOGENMIPMAP;
            mipmap_count = 0;
        }
    }

    // Create the texture.

    CComPtr<IDirect3DTexture9> texture;
    const HRESULT result = device->CreateTexture(width, height, mipmap_count, usage, d3d_format, pool, &texture, NULL);
    if (FAILED(result)) {
        logKA(MSG_ERROR, 0, "HW:Unable to create texture %08x", result);
        return NULL;
    }

    // For render targets we need special texture for transfer.

    size_t mono_height = height;
    CComPtr<IDirect3DTexture9> transfer_texture;
    CComPtr<IDirect3DTexture9> composition_texture;
    CComPtr<IDirect3DTexture9> read_16b_texture_rt;
    CComPtr<IDirect3DTexture9> read_16b_texture;
    if (render_target) {
        const HRESULT result = device->CreateTexture(width, height, 1, 0, d3d_format, D3DPOOL_SYSTEMMEM, &transfer_texture, NULL);
        if (FAILED(result)) {
            logKA(MSG_ERROR, 0, "HW:Unable to create transfer texture %08x", result);
            return NULL;
        }

        // The Nvidia 3D vision mode uses heuristic to detect which surfaces are
        // stereo and which are mono. If our composition texture is detected as stereo one,
        // the hud is missing in the left eye. For this reason we make it a square one
        // so the driver will not stereo-ize it.

        mono_height = (vision_3d && (height < width)) ? width : height;
        const HRESULT comp_result = device->CreateTexture(width, mono_height, 1, managed_usage, D3DFMT_R5G6B5, managed_memory_pool, &composition_texture, NULL);
        if (FAILED(comp_result)) {
            logKA(MSG_ERROR, 0, "HW:Unable to create composition texture %08x", comp_result);
            return NULL;
        }

        // Optional 32->16 conversion textures.

        if (is_hw_color_conversion_enabled()) {
            const HRESULT t16_result = device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, &read_16b_texture_rt, NULL);
            if (FAILED(t16_result)) {
                logKA(MSG_ERROR, 0, "HW:Unable to create 16 bit conversion render target texture %08x", t16_result);
                return NULL;
            }

            const HRESULT mt16_result = device->CreateTexture(width, height, 1, 0, D3DFMT_R5G6B5, D3DPOOL_SYSTEMMEM, &read_16b_texture, NULL);
            if (FAILED(mt16_result)) {
                logKA(MSG_ERROR, 0, "HW:Unable to create 16 bit conversion memory texture %08x", mt16_result);
                return NULL;
            }
        }
    }
    else {
        transfer_texture = texture;
    }

    // Allocate msaa render target if enabled.

    CComPtr<IDirect3DSurface9> msaa_render_target;
    if (render_target && (multisample_type != D3DMULTISAMPLE_NONE)) {
        const HRESULT result = device->CreateRenderTarget(width, height, d3d_format, multisample_type, multisample_quality, FALSE, &msaa_render_target, NULL);
        if (FAILED(result)) {
            logKA(MSG_ERROR, 0, "HW:Unable to create MSAA render target surface %08x", result);
            return NULL;
        }
    }

    // Fill the info structure.

    HWSurfaceInfo * const info = new HWSurfaceInfo();
    info->width = width;
    info->height = height;
    info->mono_height = mono_height;
    info->stride = width * 2;
    info->format = format;
    info->render_target = render_target;
    info->dx_format = d3d_format;
    info->texture = texture;
    info->texture->GetSurfaceLevel(0, &info->surface_0);
    info->transfer_texture = transfer_texture;
    info->transfer_texture->GetSurfaceLevel(0, &info->transfer_surface_0);

    info->read_16b_texture_rt = read_16b_texture_rt;
    info->read_16b_texture = read_16b_texture;
    if (info->read_16b_texture_rt) {
        info->read_16b_texture_rt->GetSurfaceLevel(0, &info->read_16b_rt_surface_0);
    }
    if (info->read_16b_texture) {
        info->read_16b_texture->GetSurfaceLevel(0, &info->read_16b_surface_0);
    }

    info->composition_texture = composition_texture;

    info->msaa_render_target = msaa_render_target;
    info->msaa_sync = HWSurfaceInfo::MSAA_SYNC_TEXTURE;
    info->cache_slot = cache_slot;

    logKA(MSG_VERBOSE, 0, "HW:create_surface %08x", info);

    // Upload data if we have any.

    if (memory) {
        update_surface(info, memory);
    }
    return info;
}

/**
 * @brief Creates a depth buffer surface.
 */
HWSurfaceHandle DX9HWLayer::create_depth_surface(const size_t width, const size_t height)
{
    D3DEVENT(L"create_depth_surface");

    // Try to pick one from lockable formats where available.

    D3DFORMAT d3d_format;
    if (multisample_type != D3DMULTISAMPLE_NONE) {
        d3d_format = D3DFMT_D24X8;
        logKA(MSG_ERROR, 0, "HW:Ignoring lockable depth buffers - multisampling");
    }
    else if (is_option_enabled("D3DEMU_NO_LOCKABLE_Z")) {
        d3d_format = D3DFMT_D24X8;
        logKA(MSG_ERROR, 0, "HW:Ignoring lockable depth buffers - D3DEMU_NO_LOCKABLE_Z");
    }
    else if (slow_zbuffer_readback) {
        d3d_format = D3DFMT_D24X8;
        logKA(MSG_ERROR, 0, "HW:Ignoring lockable depth buffers - Slow on HW");
    }
    else if (SUCCEEDED(direct3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D16_LOCKABLE))) {
        d3d_format = D3DFMT_D16_LOCKABLE;
        logKA(MSG_INFORM, 0, "HW:Using D16 - use D3DEMU_NO_LOCKABLE_Z to disable depth reads");
    }
    else if (SUCCEEDED(direct3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D32F_LOCKABLE))) {
        d3d_format = D3DFMT_D32F_LOCKABLE;
        logKA(MSG_INFORM, 0, "HW:Using D32F - use D3DEMU_NO_LOCKABLE_Z to disable depth reads");
    }
    else {
        d3d_format = D3DFMT_D24X8;
        logKA(MSG_ERROR, 0, "HW:Unable to create lockable depth surface - fake values will be returned");
    }

    CComPtr<IDirect3DSurface9> surface;
    const HRESULT result = device->CreateDepthStencilSurface(width, height, d3d_format, multisample_type, multisample_quality, FALSE, &surface, NULL);
    if (FAILED(result)) {
        logKA(MSG_ERROR, 0, "HW:Unable to create depth surface %08x", result);
        return NULL;
    }

    // Fill the info structure.

    HWSurfaceInfo * const info = new HWSurfaceInfo();
    info->width = width;
    info->height = height;
    info->stride = width * 2;
    info->format = HWFORMAT_ZBUFFER;
    info->render_target = false;
    info->dx_format = d3d_format;
    info->texture = NULL;
    info->surface_0 = surface;
    info->transfer_texture = NULL;
    info->transfer_surface_0 = surface;
    info->msaa_render_target = NULL;
    info->msaa_sync = HWSurfaceInfo::MSAA_SYNC_TEXTURE;
    info->cache_slot = 0;

    logKA(MSG_VERBOSE, 0, "HW:create_surface %08x", info);
    return info;
}

void DX9HWLayer::destroy_surface(const HWSurfaceHandle surface)
{
    assert(surface);
    D3DEVENT(L"destroy_surface");

    // Unbind the surface we are going to destroy.

    if (state.color_info == surface) {
        logKA(MSG_ERROR, 0, "HW:destroying surface 0x08x which is bound as color render target", surface);
        set_render_target(NULL, state.depth_info);
    }
    if (state.depth_info == surface) {
        logKA(MSG_ERROR, 0, "HW:destroying surface 0x08x which is bound as depth render target", surface);
        set_render_target(state.color_info, NULL);
    }

    // Insert the surface to the cache if it is cacheable. We insert it at the end
    // so it is reused last as the internal HW copy might be still in use.

    HWSurfaceInfo * const info = static_cast<HWSurfaceInfo *>(surface);
    if (info->cache_slot != 0) {

        // TODO: Maybe limit size of the cache. This should be not necessary given the difference between
        // size of memory of current GPUs, the KA/SFA requirements and the low variability of KA textures.

        if (cache[info->cache_slot].tail) {
            cache[info->cache_slot].tail->next_in_cache = info;
        }
        else {
            cache[info->cache_slot].head = info;
        }
        cache[info->cache_slot].tail = info;
    }
    else {
        delete info;
    }
}

/**
 * @brief Reads content of specified 8888 memory into destination 565.
 */
void read8888_as_565(void * const destination, const size_t pitch_dest, const void * const source, const size_t pitch_src, const size_t width, const size_t height)
{
    unsigned char * line_dest = static_cast<unsigned char *>(destination);
    const unsigned char * line_src = static_cast<const unsigned char *>(source);

    for (size_t y = 0; y < height; ++y) {

        unsigned short * dest = reinterpret_cast<unsigned short *>(line_dest);
        const unsigned int * src = reinterpret_cast<const unsigned int *>(line_src);

        // Line conversion.

        for (size_t x = 0; x < width; ++x, ++src, ++dest) {
            const unsigned int color = *src;

            const unsigned int masked_red_5 = ((color >> (5 + 3)) & 0x0000F800);
            const unsigned int masked_green_6 = ((color >> (3 + 2)) & 0x000007E0);
            const unsigned int masked_blue_5 = ((color >> (0 + 3)) & 0x0000001F);

            *dest = static_cast<unsigned short>(masked_red_5 | masked_green_6 | masked_blue_5);
        }

        // Skip unused bytes in the surfaces.

        line_dest += pitch_dest;
        line_src += pitch_src;
    }
}

/**
 * @brief Reads content of specified 8888 memory into destination 4444.
 */
void read8888_as_4444(void * const destination, const size_t pitch_dest, const void * const source, const size_t pitch_src, const size_t width, const size_t height)
{
    unsigned char * line_dest = static_cast<unsigned char *>(destination);
    const unsigned char * line_src = static_cast<const unsigned char *>(source);

    for (size_t y = 0; y < height; ++y) {

        unsigned short * dest = reinterpret_cast<unsigned short *>(line_dest);
        const unsigned int * src = reinterpret_cast<const unsigned int *>(line_src);

        // Line conversion.

        for (size_t x = 0; x < width; ++x, ++src, ++dest) {
            const unsigned int color = *src;

            const unsigned int masked_red_4 = ((color >> (8 + 4)) & 0x00000F00);
            const unsigned int masked_green_4 = ((color >> (4 + 4)) & 0x000000F0);
            const unsigned int masked_blue_4 = ((color >> (0 + 4)) & 0x0000000F);
            const unsigned int masked_alpha_4 = ((color >> (12 + 4)) & 0x0000F000);

            *dest = static_cast<unsigned short>(masked_red_4 | masked_green_4 | masked_blue_4 | masked_alpha_4);
        }

        // Skip unused bytes in the surfaces.

        line_dest += pitch_dest;
        line_src += pitch_src;
    }
}

/**
 * @brief Reads opaque 565 texture as 8888 texture.
 */
void read565_as_8888(void * const destination, const size_t pitch_dest, const void * const source, const size_t pitch_src, const size_t width, const size_t height)
{
    unsigned char * line_dest = static_cast<unsigned char *>(destination);
    const unsigned char * line_src = static_cast<const unsigned char *>(source);

    for (size_t y = 0; y < height; ++y) {

        unsigned int * dest = reinterpret_cast<unsigned int *>(line_dest);
        const unsigned short * src = reinterpret_cast<const unsigned short *>(line_src);

        for (size_t x = 0; x < width; ++x, ++src, ++dest) {
            const unsigned short color = *src;

            // Move the value to the right place.

            const unsigned int masked_red = ((color & 0x0000F800) << (5 + 3));
            const unsigned int masked_green = ((color & 0x000007E0) << (3 + 2));
            const unsigned int masked_blue = ((color & 0x0000001F) << (0 + 3));

            // Ensure proper content of low end bits so the result is not biased towards zero.

            const unsigned int replicated_red = (masked_red | (masked_red >> 5)) & 0x00ff0000;
            const unsigned int replicated_green = (masked_green | (masked_green >> 6)) & 0x0000ff00;
            const unsigned int replicated_blue = (masked_blue | (masked_blue >> 5)) & 0x000000ff;

            *dest = 0xff000000 | replicated_red | replicated_green | replicated_blue;
        }

        // Skip to next line.

        line_dest += pitch_dest;
        line_src += pitch_src;
    }
}

/**
 * @brief Reads 4444 texture as 8888 texture.
 */
void read4444_as_8888(void * const destination, const size_t pitch_dest, const void * const source, const size_t pitch_src, const size_t width, const size_t height)
{
    unsigned char * line_dest = static_cast<unsigned char *>(destination);
    const unsigned char * line_src = static_cast<const unsigned char *>(source);

    for (size_t y = 0; y < height; ++y) {

        unsigned int * dest = reinterpret_cast<unsigned int *>(line_dest);
        const unsigned short * src = reinterpret_cast<const unsigned short *>(line_src);

        for (size_t x = 0; x < width; ++x, ++src, ++dest) {
            const unsigned short color = *src;

            // Move the value to the right place.

            const unsigned int masked_red = ((color & 0x00000F00) << (8 + 4));
            const unsigned int masked_green = ((color & 0x000000F0) << (4 + 4));
            const unsigned int masked_blue = ((color & 0x0000000F) << (0 + 4));
            const unsigned int masked_alpha = ((color & 0x0000F000) << (12 + 4));

            // Ensure proper content of low end bits so the result is not biased towards zero.

            const unsigned int replicated_red = (masked_red | (masked_red >> 4));
            const unsigned int replicated_green = (masked_green | (masked_green >> 4));
            const unsigned int replicated_blue = (masked_blue | (masked_blue >> 4));
            const unsigned int replicated_alpha = (masked_alpha | (masked_alpha >> 4));

            *dest = replicated_alpha | replicated_red | replicated_green | replicated_blue;
        }

        // Skip to next line.

        line_dest += pitch_dest;
        line_src += pitch_src;
    }
}

/**
 * @brief Copies one surface to another without format conversion.
 */
void read_same_format(void * const destination, const size_t pitch_dest, const void * const source, const size_t pitch_src, const size_t width, const size_t height, const size_t texel_size)
{
        char * dest = static_cast<char *>(destination);
        const char * src = static_cast<const char *>(source);
        const size_t bytes_per_line = width * texel_size;

        for (size_t i = 0; i < height; ++i) {
            memcpy(dest, src, bytes_per_line);
            dest += pitch_dest;
            src += pitch_src;
        }
}

void DX9HWLayer::update_surface(const HWSurfaceHandle surface, const void * const memory)
{
    D3DEVENT(L"update_surface");
    assert(memory);
    logKA(MSG_VERBOSE, 0, "HW:update surface %08x from %08x", surface, memory);

    // Depth buffers are not supported for upload.

    HWSurfaceInfo * const info = static_cast<HWSurfaceInfo *>(surface);
    if (info->format == HWFORMAT_ZBUFFER) {
        logKA(MSG_ERROR, 0, "Upload of depth textures is not supported");
        return;
    }
    assert(info->format != HWFORMAT_ZBUFFER);

    // Render targets are handled in special way.

    if (info->render_target) {
        update_render_target(*info, memory);
        return;
    }

    // Lock the surface.

    D3DLOCKED_RECT rect;
    if (FAILED(log_error(info->transfer_texture->LockRect(0, &rect, NULL, 0)))) {
        return;
    }

    // Copy all lines.

    if (info->dx_format == D3DFMT_X8R8G8B8) {
        assert(info->format == HWFORMAT_R5G6B5);
        read565_as_8888(rect.pBits, rect.Pitch, memory, info->stride, info->width, info->height);
    }
    else if (info->dx_format == D3DFMT_A8R8G8B8) {
        assert(info->format == HWFORMAT_R4G4B4A4);
        read4444_as_8888(rect.pBits, rect.Pitch, memory, info->stride, info->width, info->height);
    }
    else {
        assert(
            ((info->format == HWFORMAT_R5G6B5) && (info->dx_format == D3DFMT_R5G6B5)) ||
            ((info->format == HWFORMAT_R4G4B4A4) && (info->dx_format == D3DFMT_A4R4G4B4))
        );
        read_same_format(rect.pBits, rect.Pitch, memory, info->stride, info->width, info->height, 2);
    }

    // Done.

    log_error(info->transfer_texture->UnlockRect(0));

    // Do upload to the target texture if necessary.

    if (info->transfer_texture != info->texture) {
        log_error(device->UpdateTexture(info->transfer_texture, info->texture));
    }

    // Ensure that the driver will regenerate the texture.

    else {
        info->texture->SetAutoGenFilterType(D3DTEXF_POINT);
        info->texture->SetAutoGenFilterType(D3DTEXF_LINEAR);
    }
}

void DX9HWLayer::read_surface(const HWSurfaceHandle surface, void * const memory)
{
    D3DEVENT(L"read_surface");
    logKA(MSG_VERBOSE, 0, "HW:read surface %08x to %08x", surface, memory);
    HWSurfaceInfo * const info = static_cast<HWSurfaceInfo *>(surface);

    // Special handling for depth buffers and render targets.

    if (info->format == HWFORMAT_ZBUFFER) {
        read_depth_surface(*info, memory);
        return;
    }
    else if (info->render_target) {
        read_render_target(*info, memory);
        return;
    }

    // Dedicated transfer textures are used only for render targets.

    assert(info->transfer_texture == info->texture);

    // Lock the surface.

    D3DLOCKED_RECT rect;
    if (FAILED(log_error(info->transfer_surface_0->LockRect(&rect, NULL, D3DLOCK_READONLY)))) {
        return;
    }

    // Copy all lines.

    if (info->dx_format == D3DFMT_X8R8G8B8) {
        assert(info->format == HWFORMAT_R5G6B5);
        read8888_as_565(memory, info->stride, rect.pBits, rect.Pitch, info->width, info->height);
    }
    else if (info->dx_format == D3DFMT_A8R8G8B8) {
        assert(info->format == HWFORMAT_R4G4B4A4);
        read8888_as_4444(memory, info->stride, rect.pBits, rect.Pitch, info->width, info->height);
    }
    else {
        assert(
            ((info->format == HWFORMAT_R5G6B5) && (info->dx_format == D3DFMT_R5G6B5)) ||
            ((info->format == HWFORMAT_R4G4B4A4) && (info->dx_format == D3DFMT_A4R4G4B4))
        );
        read_same_format(memory, info->stride, rect.pBits, rect.Pitch, info->width, info->height, 2);
    }

    // Done.

    log_error(info->transfer_surface_0->UnlockRect());
}

/**
 * @brief Composes memory belonging to specified surface on top of the render target surface.
 */
void DX9HWLayer::compose_render_target(const HWSurfaceHandle surface, const void * const memory, const float * const color_key)
{
    D3DEVENT(L"compose_render_target");
    logKA(MSG_VERBOSE, 0, "HW:compose_render_target: %p %p", surface, memory);

    assert(surface);
    HWSurfaceInfo * const info = static_cast<HWSurfaceInfo *>(surface);
    compose_or_update_render_target(*info, memory, false, color_key);
}

/**
 * @brief Composes memory belonging to specified surface on top of the render target surface or updates
 * it using full overwrite.
 *
 * The color_key is only supported for composition mode.
 */
void DX9HWLayer::compose_or_update_render_target(HWSurfaceInfo &info, const void * const memory, const bool update, const float * const color_key)
{
    D3DEVENT(L"compose_or_update_render_target");
    assert(info.render_target);

    // Transfer the data to the composition surface.

    const RECT lock_rect = {0, 0, info.width, info.height};
    const bool use_lock_rect = info.mono_height != info.height;

    D3DLOCKED_RECT rect;
    if (FAILED(log_error(info.composition_texture->LockRect(0, &rect, use_lock_rect ? &lock_rect : NULL, 0)))) {
        return;
    }

    // Copy entire content.

    read_same_format(rect.pBits, rect.Pitch, memory, info.stride, info.width, info.height, 2);

    log_error(info.composition_texture->UnlockRect(0));

    // Remember state so we can restore it easily.

    const HWState old_state = state;

    // Apply our state.

    set_depth_test(DEPTH_TEST_NONE);
    set_alpha_test(update ? ALPHA_TEST_NONE : ALPHA_TEST_NOEQUAL);
    set_alpha_blend(BLEND_NONE);
    set_fog(FOG_NONE, 0);
    set_flat_blend(false);
    set_texture_blend(TEXTURE_BLEND_MODULATE);
    set_texture_surface(NULL);

    // Setup the shader and corresponding constants.

    if (update) {
        activate_shader_combination(SHADER_COPY);
    }
    else {
        if (! color_key) {
            activate_shader_combination(SHADER_COMPOSE);
        }
        else {
            const float color[4] = {color_key[0], color_key[1], color_key[2], 0.0f };
            log_error(device->SetPixelShaderConstantF(10, color, 1));
            activate_shader_combination(SHADER_COMPOSE_NON_BLACK_KEY);
        }
    }

    // Bind proper output.

    if (! update) {

        // The composition will operate on top of whatever buffer contains the most updated state.

        if (info.msaa_sync == HWSurfaceInfo::MSAA_SYNC_TEXTURE) {
            log_error(device->SetRenderTarget(0, info.surface_0));
        }
        else {
            assert(info.msaa_render_target);
            log_error(device->SetRenderTarget(0, info.msaa_render_target));
            info.msaa_sync = HWSurfaceInfo::MSAA_SYNC_RT;
        }
        log_error(device->SetDepthStencilSurface(NULL));
    }
    else {

        // The update will redefine the texture as containing the most relevant content as this
        // is the most probable scenario in the 2d modes where the render target is updated directly.

        log_error(device->SetRenderTarget(0, info.surface_0));
        log_error(device->SetDepthStencilSurface(NULL));
        info.msaa_sync = HWSurfaceInfo::MSAA_SYNC_TEXTURE;
    }

    // Do custom overrides not supported by our setters.

    log_error(device->SetTexture(0, info.composition_texture));

    // Draw the geometry.

    draw_fullscreen_quad(width, height, 0.0f, 0.0f, 1.0f, static_cast<float>(info.height) / static_cast<float>(info.mono_height));

    // Restore previous state.

    log_error(device->SetTexture(0, NULL));
    apply_state(old_state, false);
}

/**
 * @brief Sets content of specified render target surface with color space conversion.
 */
void DX9HWLayer::update_render_target(HWSurfaceInfo &info, const void * const memory)
{
    D3DEVENT(L"update_render_target");
    assert(memory);
    assert(info.render_target);

    // Special handling with HW color conversion.

    if (is_hw_color_conversion_enabled()) {
        compose_or_update_render_target(info, memory, true, NULL);
        return;
    }

    // Lock the surface.

    D3DLOCKED_RECT rect;
    if (FAILED(log_error(info.transfer_texture->LockRect(0, &rect, NULL, 0)))) {
        return;
    }

    // Copy all lines.

    read565_as_8888(rect.pBits, rect.Pitch, memory, info.stride, info.width, info.height);

    // Done.

    log_error(info.transfer_texture->UnlockRect(0));

    // Do upload to the target texture.

    log_error(device->UpdateTexture(info.transfer_texture, info.texture));

    // The master copy is now the texture. Upload will happen
    // only if necessary.

    info.msaa_sync = HWSurfaceInfo::MSAA_SYNC_TEXTURE;
}

/**
 * @brief Reads a render target surface with color space conversion.
 */
void DX9HWLayer::read_render_target(HWSurfaceInfo &info, void * const memory)
{
    D3DEVENT(L"read_render_target");
    assert(info.render_target);

    // Fetch data from the mssa copy to the texture copy if necessary.

    synchronize_texture(info);

    IDirect3DSurface9 * source_surface = info.surface_0;
    IDirect3DSurface9 * transfer_surface = info.transfer_surface_0;
    bool native_transfer = false;

    // Apply the 32bit->16bit conversion in hw if possible.

    assert(info.format == HWFORMAT_R5G6B5);
    assert(info.transfer_texture != info.texture);

    if (create_16bit_copy(info)) {
        source_surface = info.read_16b_rt_surface_0;
        transfer_surface = info.read_16b_surface_0;
        native_transfer = true;
    }

    // Copy data to the transfer texture. The render targets are not lockable.

    if (FAILED(log_error(device->GetRenderTargetData(source_surface, transfer_surface)))) {
        return;
    }

    // Lock the surface.

    D3DLOCKED_RECT rect;
    if (FAILED(log_error(transfer_surface->LockRect(&rect, NULL, D3DLOCK_READONLY)))) {
        return;
    }

    // Copy all lines.

    if (native_transfer) {
        read_same_format(memory, info.stride, rect.pBits, rect.Pitch, info.width, info.height, 2);
    }
    else {
        read8888_as_565(memory, info.stride, rect.pBits, rect.Pitch, info.width, info.height);
    }

    // Done.

    log_error(transfer_surface->UnlockRect());
}

/**
 * @brief Read a depth surface while doing format conversion as necessary.
 */
void DX9HWLayer::read_depth_surface(HWSurfaceInfo &info, void * const memory)
{
    D3DEVENT(L"read_depth_surface");
    assert(info.format == HWFORMAT_ZBUFFER);

    // If the format is not lockable, simulate read of maximal values.

    if (info.dx_format == D3DFMT_D24X8) {
        memset(memory, 0xff, (info.width * info.height * 2));
        return;
    }

    // Lock it.

    D3DLOCKED_RECT rect;
    if (FAILED(log_error(info.surface_0->LockRect(&rect, NULL, D3DLOCK_READONLY)))) {
        return;
    }

    // The D16 format directly contains all the values we need.

    if (info.dx_format == D3DFMT_D16_LOCKABLE) {
        read_same_format(memory, info.stride, rect.pBits, rect.Pitch, info.width, info.height, 2);
        log_error(info.surface_0->UnlockRect());
        return;
    }

    // Conversion of the values.

    assert(info.dx_format == D3DFMT_D32F_LOCKABLE);

    // Copy all lines.

    unsigned char * line_dest = static_cast<unsigned char *>(memory);
    const unsigned char * line_src = static_cast<const unsigned char *>(rect.pBits);

    for (size_t y = 0; y < info.height; ++y) {

        unsigned short * dest = reinterpret_cast<unsigned short *>(line_dest);
        const float * src = reinterpret_cast<const float *>(line_src);

        for (size_t x = 0; x < info.width; ++x, ++src, ++dest) {

            const float input = (*src * 65535.0f);

            // Ensure that everything interesting is in the present bits of the mantisa
            // by introducing a fake bit.

            const float input_with_fake_first_bit = input + 65536.0f;

            // Read bits from the mantisa.

            const unsigned int mantisa_bits = 23;
            const unsigned int mantisa_mask = ((1 << mantisa_bits) - 1);
            const unsigned int value = ((*reinterpret_cast<const unsigned int *>(&input_with_fake_first_bit)) & mantisa_mask) >> (mantisa_bits - 16);

            *dest = static_cast<unsigned short>(value);
        }

        // Skip unused bytes in the input surface.

        dest += info.stride;
        src += rect.Pitch;
    }

    // Done.

    log_error(info.surface_0->UnlockRect());
}

void DX9HWLayer::set_depth_test(const DepthTest test)
{
    if (state.depth_test != test) {
        set_depth_test_internal(test);
    }
}

void DX9HWLayer::set_alpha_test(const AlphaTest test)
{
    if (state.alpha_test != test) {
        set_alpha_test_internal(test);
    }
}

void DX9HWLayer::set_alpha_blend(const Blend blend)
{
    if (state.alpha_blend != blend) {
        set_alpha_blend_internal(blend);
    }
}

void DX9HWLayer::set_fog(const Fog fog, const unsigned int color)
{
    if ((state.fog_mode != fog) || (state.fog_color != color)) {
        set_fog_internal(fog, color);
    }
}

void DX9HWLayer::set_flat_blend(const bool enabled)
{
    if (state.flat != enabled) {
        set_flat_blend_internal(enabled);
    }
}

void DX9HWLayer::set_texture_blend(const TextureBlend blend)
{
    if (state.texture_blend != blend) {
        set_texture_blend_internal(blend);
    }
}

void DX9HWLayer::set_texture_surface(const HWSurfaceHandle surface)
{
    if (surface == NULL) {
        if (state.texture != NULL) {
            set_texture_surface_internal(NULL);
        }
    }
    else {
        HWSurfaceInfo * const info = static_cast<HWSurfaceInfo *>(surface);
        if (state.texture != info->texture) {
            set_texture_surface_internal(info);
        }
    }
}

void DX9HWLayer::set_render_target(const HWSurfaceHandle color, const HWSurfaceHandle depth)
{
    D3DEVENT(L"set_render_target");
    logKA(MSG_VERBOSE, 0, "HW:set_render_target %08x %08x", color, depth);

    // Do not filter redundant sets, rest of the code depends on this behavior
    // for state restore and viewport reset.

    if (color == INVALID_SURFACE_HANDLE) {
        state.color_info = NULL;
        log_error(device->SetRenderTarget(0, default_color));
    }
    else {
        HWSurfaceInfo * const info = static_cast<HWSurfaceInfo *>(color);
        state.color_info = info;
        log_error(device->SetRenderTarget(0, info->msaa_render_target ? info->msaa_render_target : info->surface_0));
    }

    if (depth == INVALID_SURFACE_HANDLE) {
        state.depth_info = NULL;
        log_error(device->SetDepthStencilSurface(default_depth));
    }
    else {
        HWSurfaceInfo * const info = static_cast<HWSurfaceInfo *>(depth);
        state.depth_info = info;
        assert(! info->msaa_render_target);
        log_error(device->SetDepthStencilSurface(info->surface_0));
    }

    // Upload the size. The viewport currently covers entire area.

    if (state.color_info) {
        const float values[4] = {static_cast<float>(state.color_info->width), static_cast<float>(state.color_info->height), 0.0f, 0.0f};
        log_error(device->SetVertexShaderConstantF(2, values, 1));
    }
}

/**
 * @brief Real implementations of the setters.
 *
 * The state is always applied regardless of its previous value.
 */
//@{
void DX9HWLayer::set_depth_test_internal(const DepthTest test)
{
    state.depth_test = test;

    log_error(device->SetRenderState(D3DRS_ZENABLE, (test != DEPTH_TEST_NONE) ? TRUE : FALSE));
    log_error(device->SetRenderState(D3DRS_ZWRITEENABLE, (test != DEPTH_TEST_NOZWRITE) ? TRUE : FALSE));
}

void DX9HWLayer::set_alpha_test_internal(const AlphaTest test)
{
    state.alpha_test = test;

    log_error(device->SetRenderState(D3DRS_ALPHATESTENABLE, (test != ALPHA_TEST_NONE) ? TRUE : FALSE));
}

void DX9HWLayer::set_alpha_blend_internal(const Blend blend)
{
    state.alpha_blend = blend;

    if (blend == BLEND_NONE) {
        log_error(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
        return;
    }

    log_error(device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
    if (blend == BLEND_OVER) {
        log_error(device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
        log_error(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));
    }
    else {
        assert(blend == BLEND_ADD);
        log_error(device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE));
        log_error(device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE));
    }
}

void DX9HWLayer::set_fog_internal(const Fog fog, const unsigned int color)
{
    state.fog_mode = fog;
    state.fog_color = color;

    // Upload the color.

    const float color_constant[4] = {
        ((color >> 16) & 0xff) / 255.0f,
        ((color >> 8) & 0xff) / 255.0f,
        ((color >> 0) & 0xff) / 255.0f,
        0.0f
    };
    log_error(device->SetPixelShaderConstantF(0, color_constant, 1));

    // Upload the range.

    const float range[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    log_error(device->SetVertexShaderConstantF(1, range, 1));

    // Type is handled by shader selection.
}

void DX9HWLayer::set_flat_blend_internal(const bool enabled)
{
    state.flat = enabled;

    log_error(device->SetRenderState(D3DRS_SHADEMODE, (enabled ? D3DSHADE_FLAT : D3DSHADE_GOURAUD)));
}

void DX9HWLayer::set_texture_blend_internal(const TextureBlend blend)
{
    state.texture_blend = blend;

    // Blend is handled by shader selection.
}

void DX9HWLayer::set_texture_surface_internal(const HWSurfaceInfo * const surface)
{
    if (surface == NULL) {
        state.texture = NULL;
    }
    else {
        state.texture = surface->texture;
    }
    log_error(device->SetTexture(0, state.texture));
}
//@}

void DX9HWLayer::clear(const RECT &rect, const bool color, const bool depth, const DWORD color_value, const float depth_value)
{
    D3DEVENT(L"clear");

    // Ensure that the MSAA RT is properly updated. Note that
    // if entire surface is covered by the rectangle, the content
    // will be overwritten so there is no need to upload it.

    if (state.color_info && state.color_info->msaa_render_target) {
        if ((rect.left != 0) || (rect.right != static_cast<long>(state.color_info->width)) || (rect.top != 0) || (rect.bottom != static_cast<long>(state.color_info->height))) {
            synchronize_msaa(*state.color_info);
        }
        state.color_info->msaa_sync = HWSurfaceInfo::MSAA_SYNC_RT;
    }

    // Clear.

    D3DRECT dx_rect;
    dx_rect.x1 = rect.left;
    dx_rect.y1 = rect.top;
    dx_rect.x2 = rect.right;
    dx_rect.y2 = rect.bottom;

    log_error(device->Clear(
        1,
        &dx_rect,
        (color ? D3DCLEAR_TARGET : 0) + ((depth && state.depth_info) ? D3DCLEAR_ZBUFFER : 0),
        color_value,
        depth_value,
        0
    ));
}

void DX9HWLayer::set_triangle_vertices(const TLVertex * const vertices, const size_t count)
{
    D3DEVENT(L"set_triangle_vertices");
    if (vertex_buffer != NULL) {
        vertex_data_start_index = fill_buffer(*vertex_buffer, vertex_buffer_free_index, MAXIMAL_VERTEX_COUNT, vertices, count);
    }
}

void DX9HWLayer::draw_triangles(const TLVertex * const vertices, const size_t vertex_start, const size_t vertex_count, const unsigned short * const indices, const size_t triangle_count)
{
    D3DEVENT(L"draw_triangles");
    assert(vertices);
    assert(indices);

    // Upload render target texture content to the MSAA render target if necessary.

    if (state.color_info && state.color_info->msaa_render_target) {
        synchronize_msaa(*state.color_info);
        state.color_info->msaa_sync = HWSurfaceInfo::MSAA_SYNC_RT;
    }

    // Activate the proper shader.

    const int index = get_shader_index((state.texture != NULL), state.texture_blend, state.fog_mode);
    activate_shader_combination(index);

    // Draw.

    if (vertex_buffer == NULL) {
        log_error(device->DrawIndexedPrimitiveUP(
            D3DPT_TRIANGLELIST,
            vertex_start,
            vertex_count,
            triangle_count,
            indices,
            D3DFMT_INDEX16,
            vertices,
            sizeof(TLVertex)
        ));
        return;
    }

    const size_t starting_index = fill_buffer(*index_buffer, index_buffer_free_index, MAXIMAL_INDEX_COUNT, indices, (triangle_count * 3));
    log_error(device->DrawIndexedPrimitive(
        D3DPT_TRIANGLELIST,
        vertex_data_start_index,
        vertex_start,
        vertex_count,
        starting_index,
        triangle_count
    ));
}

void DX9HWLayer::draw_lines(const TLVertex * const vertices, const size_t vertex_start, const size_t vertex_count, const unsigned short * const indices, const size_t line_count)
{
    D3DEVENT(L"draw_lines");
    assert(vertices);
    assert(indices);

    // Upload render target texture content to the MSAA render target if necessary.

    if (state.color_info && state.color_info->msaa_render_target) {
        synchronize_msaa(*state.color_info);
        state.color_info->msaa_sync = HWSurfaceInfo::MSAA_SYNC_RT;
    }

    // Activate the proper shader.

    const int index = get_shader_index((state.texture != NULL), state.texture_blend, state.fog_mode);
    activate_shader_combination(index);

    // Draw.

    if (vertex_buffer == NULL) {
        log_error(device->DrawIndexedPrimitiveUP(
            D3DPT_LINELIST,
            vertex_start,
            vertex_count,
            line_count,
            indices,
            D3DFMT_INDEX16,
            vertices,
            sizeof(TLVertex)
        ));
        return;
    }

    const size_t starting_index = fill_buffer(*index_buffer, index_buffer_free_index, MAXIMAL_INDEX_COUNT, indices, (line_count * 2));
    log_error(device->DrawIndexedPrimitive(
        D3DPT_LINELIST,
        vertex_data_start_index,
        vertex_start,
        vertex_count,
        starting_index,
        line_count
    ));
}

void DX9HWLayer::draw_points(const TLVertex * const vertices, const size_t UNUSED_PARAMETER(vertex_start), const size_t UNUSED_PARAMETER(vertex_count), const unsigned short * const indices, const size_t point_count)
{
    D3DEVENT(L"draw_points");
    assert(vertices);
    assert(indices);
    if (point_count == 0) {
        return;
    }

    // Upload render target texture content to the MSAA render target if necessary.

    if (state.color_info && state.color_info->msaa_render_target) {
        synchronize_msaa(*state.color_info);
        state.color_info->msaa_sync = HWSurfaceInfo::MSAA_SYNC_RT;
    }

    // Activate the proper shader.

    const int index = get_shader_index((state.texture != NULL), state.texture_blend, state.fog_mode);
    activate_shader_combination(index);

    // Draw the primitive. Indexed point lists are not supported by API so we need to process this by contiguous regions.
    // Prepare start of first block.

    size_t base = indices[0];
    size_t count = 1;

    for (size_t current = 1; current < point_count; ++current) {

        // Additional indices must directly append to the end of the block.

        const size_t current_index = indices[current];
        if (current_index == (base + count)) {
            count++;
            continue;
        }

        // Flush the previous contiguous block.

        log_error(device->DrawPrimitiveUP(
            D3DPT_POINTLIST,
            count,
            vertices + base,
            sizeof(TLVertex)
        ));

        // Start a new block.

        base = current_index;
        count = 1;
    }

    // Flush last unfinished block.

    if (count > 0) {
        log_error(device->DrawPrimitiveUP(
            D3DPT_POINTLIST,
            count,
            vertices + base,
            sizeof(TLVertex)
        ));
    }

    // Restore stream binding changed by the API.

    bind_buffers();
}

void DX9HWLayer::bitblt(const HWSurfaceHandle destination, const HWSurfaceHandle source, const size_t x, const size_t y, const size_t src_x, const size_t src_y, const size_t src_width, const size_t src_height)
{
    D3DEVENT(L"bitblt");
    logKA(MSG_VERBOSE, 0, "HW:bitblt %08x -> %08x", source, destination);

    // Ensure that we have content inside the textures.

    HWSurfaceInfo * const destination_info = static_cast<HWSurfaceInfo *>(destination);
    synchronize_texture(*destination_info);

    HWSurfaceInfo * const source_info = static_cast<HWSurfaceInfo *>(source);
    synchronize_texture(*source_info);

    // Remember state so we can restore it easily.

    const HWState old_state = state;

    // Apply our state.

    set_depth_test(DEPTH_TEST_NONE);
    set_alpha_test(ALPHA_TEST_NONE);
    set_alpha_blend(BLEND_NONE);
    set_fog(FOG_NONE, 0);
    set_flat_blend(false);
    set_texture_blend(TEXTURE_BLEND_MODULATE);
    set_texture_surface(source);
    set_render_target(INVALID_SURFACE_HANDLE, INVALID_SURFACE_HANDLE);

    // Bind the normal texture to the output.

    log_error(device->SetRenderTarget(0, destination_info->surface_0));
    log_error(device->SetDepthStencilSurface(NULL));
    destination_info->msaa_sync = HWSurfaceInfo::MSAA_SYNC_TEXTURE;

    // The copy shader.

    activate_shader_combination(SHADER_COPY);

    // Set the target viewport to cover the target area.

    D3DVIEWPORT9 vp;
    vp.X      = x;
    vp.Y      = y;
    vp.Width  = src_width;
    vp.Height = src_height;
    vp.MinZ   = 0.0f;
    vp.MaxZ   = 1.0f;
    log_error(device->SetViewport(&vp));

    // UV coordinates for the source area.

    const float txt_left = static_cast<float>(src_x) / static_cast<float>(source_info->width);
    const float txt_top = static_cast<float>(src_y) / static_cast<float>(source_info->height);
    const float txt_right = static_cast<float>(src_x + src_width) / static_cast<float>(source_info->width);
    const float txt_bottom = static_cast<float>(src_y + src_height) / static_cast<float>(source_info->height);

    // Draw.

    draw_fullscreen_quad(src_width, src_height, txt_left, txt_top, txt_right, txt_bottom);

    // Restore previous state. The state resets the render target
    // which resets the viewport to entire area.

    apply_state(old_state, false);
}

void DX9HWLayer::display_surface(const HWSurfaceHandle surface)
{
    D3DEVENT(L"display_surface");
    logKA(MSG_VERBOSE, 0, "HW:display_surface %08x", surface);

    // Ensure that we have content in the texture if the MSAA buffer
    // is valid.

    HWSurfaceInfo * const info = static_cast<HWSurfaceInfo *>(surface);
    synchronize_texture(*info);

    // Remember state so we can restore it easily.

    const HWState old_state = state;

    // Apply our state.

    set_depth_test(DEPTH_TEST_NONE);
    set_alpha_test(ALPHA_TEST_NONE);
    set_alpha_blend(BLEND_NONE);
    set_fog(FOG_NONE, 0);
    set_flat_blend(false);
    set_texture_blend(TEXTURE_BLEND_MODULATE);
    set_texture_surface(surface);
    set_render_target(INVALID_SURFACE_HANDLE, INVALID_SURFACE_HANDLE);

    // Special presentation shader.

    activate_shader_combination(SHADER_PRESENT);

    // Draw the content.

    draw_fullscreen_quad(width, height);

    // Flip the surface.

    log_error(device->Present(NULL, NULL, NULL, NULL));

    // Restore previous state.

    apply_state(old_state, false);
}

/**
 * @brief Ensures that the RT texture contains valid content, synchronizing it
 * from the MSAA RT if necessary.
 */
void DX9HWLayer::synchronize_texture(HWSurfaceInfo &info)
{
    if ((info.msaa_render_target == NULL) || (info.msaa_sync != HWSurfaceInfo::MSAA_SYNC_RT)) {
        return;
    }
    D3DEVENT(L"synchronize_texture");
    logKA(MSG_VERBOSE, 0, "HW:reading 0x%08x from MSAA RT", &info);

    assert(info.format != HWFORMAT_ZBUFFER);
    log_error(device->StretchRect(info.msaa_render_target, NULL, info.surface_0, NULL, D3DTEXF_NONE));
    info.msaa_sync = HWSurfaceInfo::MSAA_SYNC_BOTH;
}

/**
 * @brief Ensures that the MSAA RT contains valid content, synchronizing it
 * from the RT texture if necessary.
 */
void DX9HWLayer::synchronize_msaa(HWSurfaceInfo &info)
{
    if ((info.msaa_render_target == NULL) || (info.msaa_sync != HWSurfaceInfo::MSAA_SYNC_TEXTURE)) {
        return;
    }
    D3DEVENT(L"synchronize_msaa");
    logKA(MSG_VERBOSE, 0, "HW:uploading 0x%08x to MSAA RT", &info);

    assert(info.format != HWFORMAT_ZBUFFER);
    log_error(device->StretchRect(info.surface_0, NULL, info.msaa_render_target, NULL, D3DTEXF_NONE));
    info.msaa_sync = HWSurfaceInfo::MSAA_SYNC_BOTH;
}

/**
 * @brief Draws quad over entire viewport which has specified dimensions.
 */
void DX9HWLayer::draw_fullscreen_quad(const size_t viewport_width, const size_t viewport_height)
{
    draw_fullscreen_quad(viewport_width, viewport_height, 0.0f, 0.0f, 1.0f, 1.0f);
}

/**
 * @brief Draws fullscreen quad into viewport of specified dimensions.
 *
 * @pre The viewport is already set.
 * @pre The pixel to texel mapping requires that no scaling is used.
 */
void DX9HWLayer::draw_fullscreen_quad(const size_t viewport_width, const size_t viewport_height, const float txt_left, const float txt_top, const float txt_right, const float txt_bottom)
{
    D3DEVENT(L"draw_fullscreen_quad");

    // Prepare vertices.

    const float correction_w = 2.0f * 0.5f / static_cast<float>(viewport_width);
    const float correction_h = 2.0f * 0.5f / static_cast<float>(viewport_height);

    const struct {
        float x,y, z, u,v;
    } vertices[4] = {
        { -1.0f - correction_w, -1.0f + correction_h, 0.0f, txt_left, txt_bottom},
        { -1.0f - correction_w,  1.0f + correction_h, 0.0f, txt_left, txt_top},
        {  1.0f - correction_w,  1.0f + correction_h, 0.0f, txt_right, txt_top},
        {  1.0f - correction_w, -1.0f + correction_h, 0.0f, txt_right, txt_bottom},
    };

    // Draw.

    if (! scene_active) {
        log_error(device->BeginScene());
    }
    log_error(device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0)));
    log_error(device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(vertices[0])));
    log_error(device->SetFVF(vision_3d ? STANDARD_FVF_VISION : STANDARD_FVF_NORMAL));
    if (! scene_active) {
        log_error(device->EndScene());
    }

    // Restore stream binding.

    bind_buffers();
}

/**
 * @brief Applies default rendering state.
 */
void DX9HWLayer::set_default_states(void)
{
    D3DEVENT(L"set_default_states");

    // Apply default state.

    log_error(device->SetRenderState(D3DRS_ALPHAREF, 0));
    log_error(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
    log_error(device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID));
    log_error(device->SetRenderState(D3DRS_LASTPIXEL, FALSE));
    log_error(device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL));

    log_error(device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
    log_error(device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR));
    log_error(device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR));

    if (max_anisotropy > 1) {
        log_error(device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC));
        log_error(device->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, max_anisotropy));
    }

    // Point drawing.

    float point_size = 1.0;
    log_error(device->SetRenderState(D3DRS_POINTSCALEENABLE, FALSE));
    log_error(device->SetRenderState(D3DRS_POINTSIZE, *reinterpret_cast<DWORD *>(&point_size)));

    // State which is fixed in the non-default state.

    log_error(device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_NOTEQUAL));

    // FVF for use with the game geometry.

    log_error(device->SetFVF(vision_3d ? STANDARD_FVF_VISION : STANDARD_FVF_NORMAL));

    // Initial setup buffer binding.

    bind_buffers();

    // State whose default is different from the API default.

    HWState state;
    apply_state(state, true);
}

/**
 * @brief Activates specified combination of shaders while filtering
 * redundant operations
 */
void DX9HWLayer::activate_shader_combination(const int index)
{
    if (index == active_combination) {
        return;
    }
    active_combination =  index;
    D3DEVENT(L"activate_shader_combination");

    if (index == -1) {
        log_error(device->SetVertexShader(NULL));
        log_error(device->SetPixelShader(NULL));
    }
    else {
        assert((index >= 0) && (index < DX9_SHADER_VARIANT_COUNT));
        log_error(device->SetVertexShader(vertex_shaders[index]));
        log_error(device->SetPixelShader(fragment_shaders[index]));
    }
}

/**
 * @brief Applies all states from specified structure.
 */
void DX9HWLayer::apply_state(const HWState &state, const bool force)
{
    D3DEVENT(L"apply_state");
    if (! force) {
        set_depth_test(state.depth_test);
        set_alpha_test(state.alpha_test);
        set_alpha_blend(state.alpha_blend);
        set_fog(state.fog_mode, state.fog_color);
        set_flat_blend(state.flat);
        set_texture_blend(state.texture_blend);
    }
    else {
        set_depth_test_internal(state.depth_test);
        set_alpha_test_internal(state.alpha_test);
        set_alpha_blend_internal(state.alpha_blend);
        set_fog_internal(state.fog_mode, state.fog_color);
        set_flat_blend_internal(state.flat);
        set_texture_blend_internal(state.texture_blend);
    }

    // Directly set states for which there is no proper setter.

    this->state.texture = state.texture;
    log_error(device->SetTexture(0, state.texture));

    // Set the render targets.

    set_render_target(state.color_info, state.depth_info);
}

/**
 * @brief Restores binding of the buffers.
 */
void DX9HWLayer::bind_buffers(void)
{
    if (vertex_buffer != NULL) {
        log_error(device->SetStreamSource(0, vertex_buffer, 0, sizeof(TLVertex)));
    }
    if (index_buffer != NULL) {
        log_error(device->SetIndices(index_buffer));
    }
}

/**
 * @brief Creates a 16bit copy of specified render target surface.
 *
 * The copy will be stored in the read_16b_texture if available. Otherwise
 * the function will return false and do nothing.
 */
bool DX9HWLayer::create_16bit_copy(HWSurfaceInfo &info)
{
    if (! info.read_16b_texture_rt) {
        return false;
    }

    D3DEVENT(L"create_16bit_copy");
    logKA(MSG_VERBOSE, 0, "HW:create_16bit_copy: %p", &info);

    // Ensure that the texture contains a valid content if msaa is used.

    synchronize_texture(info);

    // Remember state so we can restore it easily.

    const HWState old_state = state;

    // Apply our state.

    set_depth_test(DEPTH_TEST_NONE);
    set_alpha_test(ALPHA_TEST_NONE);
    set_alpha_blend(BLEND_NONE);
    set_fog(FOG_NONE, 0);
    set_flat_blend(false);
    set_texture_blend(TEXTURE_BLEND_MODULATE);
    set_texture_surface(&info);
    activate_shader_combination(SHADER_COPY);

    // Custom setup with no appropriate setter.

    log_error(device->SetRenderTarget(0, info.read_16b_rt_surface_0));
    log_error(device->SetDepthStencilSurface(NULL));

    // Draw the geometry.

    draw_fullscreen_quad(width, height);

    // Restore previous state. The render target binding is
    // always reset so there is no need to restore some default
    // state.

    apply_state(old_state, false);
    return true;
}

void DX9HWLayer::start_event(const wchar_t * const name)
{
    REFERENCE(name);
#ifdef HW_EVENTS
    D3DPERF_BeginEvent(0, name);
#endif
}

void DX9HWLayer::end_event(void)
{
#ifdef HW_EVENTS
    D3DPERF_EndEvent();
#endif
}

void DX9HWLayer::marker(const wchar_t * const name)
{
    REFERENCE(name);
#ifdef HW_EVENTS
    D3DPERF_SetMarker(0, name);
#endif
}

} // namespace emu

// EOF //
