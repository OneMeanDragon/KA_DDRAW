#include "ddraw_emu.h"
#include "surface_emu.h"
#include "material_emu.h"
#include "viewport_emu.h"
#include "structure_log.h"
#include "../helpers/config.h"
#include <ddraw.h>
#include <set>
#include <assert.h>

namespace emu {
namespace {

/**
 * @brief Device capabilities to report.
 *
 * We report more than we support so need for individual features
 * can be detected.
 */
D3DDEVICEDESC device_desc = {
    sizeof(D3DDEVICEDESC),
    (
        D3DDD_BCLIPPING |
        D3DDD_COLORMODEL |
        D3DDD_DEVCAPS |
        D3DDD_DEVICERENDERBITDEPTH |
        D3DDD_DEVICEZBUFFERBITDEPTH |
        D3DDD_LIGHTINGCAPS |
        D3DDD_LINECAPS |
        D3DDD_MAXBUFFERSIZE |
        D3DDD_MAXVERTEXCOUNT |
        D3DDD_TRANSFORMCAPS |
        D3DDD_TRICAPS
    ),
    D3DCOLOR_RGB,           // Needed by KA
    (
        D3DDEVCAPS_EXECUTESYSTEMMEMORY |
        D3DDEVCAPS_EXECUTEVIDEOMEMORY |
        D3DDEVCAPS_FLOATTLVERTEX |
        D3DDEVCAPS_TEXTURESYSTEMMEMORY |
        D3DDEVCAPS_TEXTUREVIDEOMEMORY |
        D3DDEVCAPS_TLVERTEXSYSTEMMEMORY |
        D3DDEVCAPS_TLVERTEXVIDEOMEMORY
    ),
    {
        sizeof(D3DTRANSFORMCAPS),
        D3DTRANSFORMCAPS_CLIP
    },
    TRUE,
    {
        sizeof(D3DLIGHTINGCAPS),
        (
            D3DLIGHTCAPS_POINT |
            D3DLIGHTCAPS_SPOT |
            D3DLIGHTCAPS_DIRECTIONAL
        ),
        D3DLIGHTINGMODEL_RGB,
        8
    },
    {
        // Line caps.
        sizeof(D3DPRIMCAPS),
        (
            D3DPMISCCAPS_CULLCCW |
            D3DPMISCCAPS_CULLCW |
            D3DPMISCCAPS_MASKZ
        ),
        (
            D3DPRASTERCAPS_FOGVERTEX |
            D3DPRASTERCAPS_FOGTABLE |
            D3DPRASTERCAPS_SUBPIXEL |
            D3DPRASTERCAPS_ZTEST
        ),
        ( // Depth CMP
            D3DPCMPCAPS_ALWAYS |
            D3DPCMPCAPS_EQUAL |
            D3DPCMPCAPS_GREATER |
            D3DPCMPCAPS_GREATEREQUAL |
            D3DPCMPCAPS_LESS |
            D3DPCMPCAPS_LESSEQUAL |
            D3DPCMPCAPS_NEVER |
            D3DPCMPCAPS_NOTEQUAL
        ),
        ( // Src blend.
            D3DPBLENDCAPS_BOTHINVSRCALPHA |
            D3DPBLENDCAPS_BOTHSRCALPHA |
            D3DPBLENDCAPS_DESTALPHA |
            D3DPBLENDCAPS_DESTCOLOR |
            D3DPBLENDCAPS_INVDESTALPHA |
            D3DPBLENDCAPS_INVDESTCOLOR |
            D3DPBLENDCAPS_INVSRCALPHA |
            D3DPBLENDCAPS_INVSRCCOLOR |
            D3DPBLENDCAPS_ONE |
            D3DPBLENDCAPS_SRCALPHA |
            D3DPBLENDCAPS_SRCALPHASAT |
            D3DPBLENDCAPS_SRCCOLOR |
            D3DPBLENDCAPS_ZERO
        ),
        ( // Dest blend.
            D3DPBLENDCAPS_BOTHINVSRCALPHA |
            D3DPBLENDCAPS_BOTHSRCALPHA |
            D3DPBLENDCAPS_DESTALPHA |
            D3DPBLENDCAPS_DESTCOLOR |
            D3DPBLENDCAPS_INVDESTALPHA |
            D3DPBLENDCAPS_INVDESTCOLOR |
            D3DPBLENDCAPS_INVSRCALPHA |
            D3DPBLENDCAPS_INVSRCCOLOR |
            D3DPBLENDCAPS_ONE |
            D3DPBLENDCAPS_SRCALPHA |
            D3DPBLENDCAPS_SRCALPHASAT |
            D3DPBLENDCAPS_SRCCOLOR |
            D3DPBLENDCAPS_ZERO
        ),
       ( // Alpha CMP
            D3DPCMPCAPS_ALWAYS |
            D3DPCMPCAPS_EQUAL |
            D3DPCMPCAPS_GREATER |
            D3DPCMPCAPS_GREATEREQUAL |
            D3DPCMPCAPS_LESS |
            D3DPCMPCAPS_LESSEQUAL |
            D3DPCMPCAPS_NEVER |
            D3DPCMPCAPS_NOTEQUAL
        ),
        (
            D3DPSHADECAPS_ALPHAFLATBLEND |
            D3DPSHADECAPS_ALPHAGOURAUDBLEND |
            D3DPSHADECAPS_COLORFLATRGB |
            D3DPSHADECAPS_COLORGOURAUDRGB |
            D3DPSHADECAPS_FOGFLAT |
            D3DPSHADECAPS_FOGGOURAUD |
            D3DPSHADECAPS_SPECULARFLATRGB |
            D3DPSHADECAPS_SPECULARGOURAUDRGB
        ),
        (
            D3DPTEXTURECAPS_ALPHA |
            D3DPTEXTURECAPS_BORDER |
            D3DPTEXTURECAPS_PERSPECTIVE
        ),
        (
            D3DPTFILTERCAPS_LINEAR |
            D3DPTFILTERCAPS_LINEARMIPLINEAR |
            D3DPTFILTERCAPS_LINEARMIPNEAREST |
            D3DPTFILTERCAPS_MIPLINEAR |
            D3DPTFILTERCAPS_MIPNEAREST |
            D3DPTFILTERCAPS_NEAREST
        ),
        (
            D3DPTBLENDCAPS_COPY |
            D3DPTBLENDCAPS_DECAL |
            D3DPTBLENDCAPS_DECALALPHA |
            D3DPTBLENDCAPS_DECALMASK |
            D3DPTBLENDCAPS_MODULATE |
            D3DPTBLENDCAPS_MODULATEALPHA |
            D3DPTBLENDCAPS_MODULATEMASK
        ),
        (
            D3DPTADDRESSCAPS_CLAMP |
            D3DPTADDRESSCAPS_MIRROR |
            D3DPTADDRESSCAPS_WRAP
        ),
        0, //dwStippleWidth
        0  // dwStippleHeight
    },
    {
        // Triangle caps.
        sizeof(D3DPRIMCAPS),
        (
            D3DPMISCCAPS_CULLCCW |
            D3DPMISCCAPS_CULLCW |
            D3DPMISCCAPS_MASKZ
        ),
        (
            D3DPRASTERCAPS_DITHER | // Needed by KA
            D3DPRASTERCAPS_FOGTABLE |
            D3DPRASTERCAPS_FOGVERTEX |
            D3DPRASTERCAPS_SUBPIXEL |
            D3DPRASTERCAPS_ZTEST
        ),
        ( // Depth CMP
            D3DPCMPCAPS_ALWAYS |
            D3DPCMPCAPS_EQUAL |
            D3DPCMPCAPS_GREATER |
            D3DPCMPCAPS_GREATEREQUAL |
            D3DPCMPCAPS_LESS |
            D3DPCMPCAPS_LESSEQUAL |
            D3DPCMPCAPS_NEVER |
            D3DPCMPCAPS_NOTEQUAL
        ),
        ( // Src blend.
            D3DPBLENDCAPS_BOTHINVSRCALPHA |
            D3DPBLENDCAPS_BOTHSRCALPHA |
            D3DPBLENDCAPS_DESTALPHA |
            D3DPBLENDCAPS_DESTCOLOR |
            D3DPBLENDCAPS_INVDESTALPHA |
            D3DPBLENDCAPS_INVDESTCOLOR |
            D3DPBLENDCAPS_INVSRCALPHA |
            D3DPBLENDCAPS_INVSRCCOLOR |
            D3DPBLENDCAPS_ONE |
            D3DPBLENDCAPS_SRCALPHA |
            D3DPBLENDCAPS_SRCALPHASAT |
            D3DPBLENDCAPS_SRCCOLOR |
            D3DPBLENDCAPS_ZERO
        ),
        ( // Dest blend.
            D3DPBLENDCAPS_BOTHINVSRCALPHA |
            D3DPBLENDCAPS_BOTHSRCALPHA |
            D3DPBLENDCAPS_DESTALPHA |
            D3DPBLENDCAPS_DESTCOLOR |
            D3DPBLENDCAPS_INVDESTALPHA |
            D3DPBLENDCAPS_INVDESTCOLOR |
            D3DPBLENDCAPS_INVSRCALPHA |
            D3DPBLENDCAPS_INVSRCCOLOR |
            D3DPBLENDCAPS_ONE |
            D3DPBLENDCAPS_SRCALPHA |
            D3DPBLENDCAPS_SRCALPHASAT |
            D3DPBLENDCAPS_SRCCOLOR |
            D3DPBLENDCAPS_ZERO
        ),
       ( // Alpha CMP
            D3DPCMPCAPS_ALWAYS |
            D3DPCMPCAPS_EQUAL |
            D3DPCMPCAPS_GREATER |
            D3DPCMPCAPS_GREATEREQUAL |
            D3DPCMPCAPS_LESS |
            D3DPCMPCAPS_LESSEQUAL |
            D3DPCMPCAPS_NEVER |
            D3DPCMPCAPS_NOTEQUAL
        ),
        (
            D3DPSHADECAPS_ALPHAFLATBLEND |
            D3DPSHADECAPS_ALPHAGOURAUDBLEND |
            D3DPSHADECAPS_COLORFLATRGB |
            D3DPSHADECAPS_COLORGOURAUDRGB |
            D3DPSHADECAPS_FOGFLAT |
            D3DPSHADECAPS_FOGGOURAUD |
            D3DPSHADECAPS_SPECULARFLATRGB |
            D3DPSHADECAPS_SPECULARGOURAUDRGB
        ),
        (
            D3DPTEXTURECAPS_ALPHA |
            D3DPTEXTURECAPS_BORDER |
            D3DPTEXTURECAPS_PERSPECTIVE
        ),
        (
            D3DPTFILTERCAPS_LINEAR |
            D3DPTFILTERCAPS_LINEARMIPLINEAR |
            D3DPTFILTERCAPS_LINEARMIPNEAREST |
            D3DPTFILTERCAPS_MIPLINEAR |
            D3DPTFILTERCAPS_MIPNEAREST |
            D3DPTFILTERCAPS_NEAREST
        ),
        (
            D3DPTBLENDCAPS_COPY |
            D3DPTBLENDCAPS_DECAL |
            D3DPTBLENDCAPS_DECALALPHA |
            D3DPTBLENDCAPS_DECALMASK |
            D3DPTBLENDCAPS_MODULATE |
            D3DPTBLENDCAPS_MODULATEALPHA |
            D3DPTBLENDCAPS_MODULATEMASK
        ),
        (
            D3DPTADDRESSCAPS_CLAMP |
            D3DPTADDRESSCAPS_MIRROR |
            D3DPTADDRESSCAPS_WRAP
        ),
        0, //dwStippleWidth
        0  // dwStippleHeight
    },
    DDBD_32,
    DDBD_16,
    0,      // execute buffer size limit
    65535   // vertex count limit
};

const DDCAPS_DX6 device_caps = {
        sizeof(DDCAPS_DX6), // // size of the structure
        ( // driver specific cap.
            DDCAPS_3D |
            DDCAPS_ALPHA
        ),
        0, // more driver specific cap.
        0, // color key cap.
        0, // stretching and effects cap.
        0, // alpha cap
        0, // palette cap.
        0, // stereo vision cap
        0, // dwAlphaBltConstBitDepths
        0, // dwAlphaBltPixelBitDepths
        0, // dwAlphaBltSurfaceBitDepths
        0, // dwAlphaOverlayConstBitDepths
        0, // dwAlphaOverlayPixelBitDepths
        0, // dwAlphaOverlaySurfaceBitDepths
        DDBD_24, // dwZBufferBitDepths
        128 * 1024 * 1024,  // total video memory
        128 * 1024 * 1024,  // free video memory
        0,  // maximum visible overlays
        0,  // current visible overlays
        0,  // number of four cc codes
        0,  // source rectangle alignment
        0,  // source rectangle byte size
        0,  // dest rectangle alignment
        0,  // dest rectangle byte size
        0,  // stride alignment
        { 0, 0, 0, 0, 0, 0, 0, 0 }, // ROPS supported
        (   // general capabilities
            DDCAPS_3D |
            DDCAPS_ALPHA
        ),
        0,  // minimum overlay stretch factor
        0,  // maximum overlay stretch factor
        0,  // minimum live video stretch factor
        0,  // maximum live video stretch factor
        0,  // minimum hardware codec stretch factor
        0,  // maximum hardware codec stretch factor
        0,  // reserved
        0,  // reserved
        0,  // reserved

        0,  // driver specific System->Vmem
        0,  // driver color key System->Vmem
        0,  // driver FX System->Vmem
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        0,  // driver specific Vmem->System
        0,  // driver color key Vmem->System
        0,  // driver FX Vmem->System
        { 0, 0, 0, 0, 0, 0, 0, 0 }, // ROPS Vmem->System
        0,  // driver specific System->System
        0,  // driver color key System->System
        0,  // driver FX System->System
        { 0, 0, 0, 0, 0, 0, 0, 0 }, // ROPS System->System
        0,  // usable video ports
        0,  // video ports used
        0,  // driver cap. System->Vmem
        0,  // driver cap. non-local->local vidmem
        0,  // driver cap. non-local->local vidmem
        0,  // driver color key non-local->local vidmem
        0,  // driver FX non-local->local
        { 0, 0, 0, 0, 0, 0, 0, 0 }, // ROPS non-local->local
        {
            DDCAPS_3D,
            0,
            0,
            0
        }
};

/**
 * @brief Function routed from the original game logKA function.
 */
void __cdecl redirected_log_body(const int level, const char * const format, va_list args)
{
    const DWORD last_error = GetLastError();

    char tmpbuff[10000];

    // logKA header.

    sprintf(tmpbuff, "KA:%u:", level);
    char * const message_start = tmpbuff + strlen(tmpbuff);

    // Format the body.

    vsprintf(message_start, format, args);

    // Pass it to our logging system.

    log_raw_line(tmpbuff);

    // Restore the last error to remain fully transparent.

    SetLastError(last_error);
}

/**
 * @brief Ensures protection of all registers from our redirection wrapper.
 */
__declspec(naked) int redirected_log(const int level, const char * const format, ...)
{
    REFERENCE(level);
    REFERENCE(format);

    __asm {

        // Standard prolog.

        push ebp
        mov ebp, esp

        // Store all registers.

        pusha

        // Simulate the vararg.

        lea eax, [ebp + 16]
        push eax

        // Fixed parameters.

        push dword ptr [ebp + 12]
        push dword ptr [ebp + 8]

        // Do it.

        call redirected_log_body
        add esp, 12

        // Restore all registers.

        popa

        // Restore ebp.

        mov esp, ebp
        pop ebp

        // Expected return value.

        mov eax, 1
        ret
    }
}


/**
 * @brief Ensures protection of all registers from our redirection wrapper.
 */
__declspec(naked) int redirected_log_no_level(const char * const format, ...)
{
    REFERENCE(format);

    __asm {

        // Standard prolog.

        push ebp
        mov ebp, esp

        // Store all registers.

        pusha

        // Simulate the vararg.

        lea eax, [ebp + 12]
        push eax

        // Fixed parameters.

        push dword ptr [ebp + 8]

        // Fake level.

        xor eax, eax
        push eax

        // Do it.

        call redirected_log_body
        add esp, 12

        // Restore all registers.

        popa

        // Restore ebp.

        mov esp, ebp
        pop ebp

        // Expected return value.

        mov eax, 1
        ret
    }
}


/**
 * @name Overrides the shading mode used for glow addition so it can be detected
 * by the wrapper. The shade mode is the first render state applied. This ensures that
 * we activate the special mode before any change could flush queued geometry.
 */
//@{
const size_t SHADE_MODE_HACK_1_ADDRESS = 0x00411FE3;
const unsigned char SHADE_MODE_HACK_1_OLD_KA[] = {0xA1, 0x74, 0xEB, 0x75, 0x00, 0xBD, 0x02, 0x00, 0x00, 0x00, 0x3B, 0xC5, 0x74, 0x12};
const unsigned char SHADE_MODE_HACK_1_NEW_KA[] = {0xA1, 0x74, 0xEB, 0x75, 0x00, 0xBD, 0x12, 0x00, 0x00, 0x00, 0x3B, 0xC5, 0x74, 0x12};

const unsigned char SHADE_MODE_HACK_1_OLD_KAAI[] = {0xA1, 0x74, 0xFB, 0x75, 0x00, 0xBD, 0x02, 0x00, 0x00, 0x00, 0x3B, 0xC5, 0x74, 0x12};
const unsigned char SHADE_MODE_HACK_1_NEW_KAAI[] = {0xA1, 0x74, 0xFB, 0x75, 0x00, 0xBD, 0x12, 0x00, 0x00, 0x00, 0x3B, 0xC5, 0x74, 0x12};

const size_t SHADE_MODE_HACK_2_ADDRESS = 0x004122DD;
const unsigned char SHADE_MODE_HACK_2_OLD_KA[] = {0xE8, 0x6E, 0xA8, 0xFF, 0xFF, 0xA1, 0x74, 0xEB, 0x75, 0x00, 0xBF, 0x01, 0x00, 0x00, 0x00, 0x3B, 0xC7};
const unsigned char SHADE_MODE_HACK_2_NEW_KA[] = {0xE8, 0x6E, 0xA8, 0xFF, 0xFF, 0xA1, 0x74, 0xEB, 0x75, 0x00, 0xBF, 0x11, 0x00, 0x00, 0x00, 0x3B, 0xC7};

const unsigned char SHADE_MODE_HACK_2_OLD_KAAI[] = {0xE8, 0x6E, 0xA8, 0xFF, 0xFF, 0xA1, 0x74, 0xFB, 0x75, 0x00, 0xBF, 0x01, 0x00, 0x00, 0x00, 0x3B, 0xC7};
const unsigned char SHADE_MODE_HACK_2_NEW_KAAI[] = {0xE8, 0x6E, 0xA8, 0xFF, 0xFF, 0xA1, 0x74, 0xFB, 0x75, 0x00, 0xBF, 0x11, 0x00, 0x00, 0x00, 0x3B, 0xC7};

//@}

/**
 * @name Redirects the body of the dummy logKA function to wrapper.
 *
 * The VALUE_TO_CHANGE in the new code need to be replaced with offset between LOG_FUNCTION_HACK_x_ADDRESS + 5 and
 * target function.
 */
//@{
const unsigned VALUE_TO_CHANGE = 0xff;
const size_t LOG_FUNCTION_HACK_1_ADDRESS = 0x004BF3E0;
const size_t LOG_FUNCTION_HACK_2_ADDRESS = 0x004BF3D0;
const unsigned char LOG_FUNCTION_HACK_OLD[] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3, 0x90, 0x90, 0x90, 0x90};
unsigned char LOG_FUNCTION_HACK_1_NEW[]     = {0xE9, VALUE_TO_CHANGE, VALUE_TO_CHANGE, VALUE_TO_CHANGE, VALUE_TO_CHANGE, 0xC3, 0x90, 0x90, 0x90, 0x90};
unsigned char LOG_FUNCTION_HACK_2_NEW[]     = {0xE9, VALUE_TO_CHANGE, VALUE_TO_CHANGE, VALUE_TO_CHANGE, VALUE_TO_CHANGE, 0xC3, 0x90, 0x90, 0x90, 0x90};
//@}

/**
 * @brief Ignores the flag forces non-zero alpha on bmp/pcx combination.
 */
const size_t ALLOW_BLACK_ALPHA_HACK_ADDRESS = 0x0046F2A8;
const unsigned char ALLOW_BLACK_ALPHA_HACK_OLD[] = {0x74, 0x09, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xD3, 0xE0, 0x0B, 0xF0};
const unsigned char ALLOW_BLACK_ALPHA_HACK_NEW[] = {0x74, 0x09, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xD3, 0xE0, 0x90, 0x90};

/**
 * @brief Support for redirection of GetTickCount to timeGetTime.
 */
//@{
bool get_tick_count_patched = false;
unsigned char get_tick_count_backup[5] = {0};
unsigned char GET_TICK_COUNT_NEW[5] = {0xE9, VALUE_TO_CHANGE, VALUE_TO_CHANGE, VALUE_TO_CHANGE, VALUE_TO_CHANGE};
//@}

/**
 * @brief Compares existing code at specified address with original code and replaces it with the new one.
 */
template<size_t size>
static bool patch_code(const size_t code_address, const unsigned char (&original)[size], const unsigned char (&patch)[size])
{
    void * const dest_memory = reinterpret_cast<void *>(code_address);

    // Check the memory. If we are attached to a unexpected process (launcher for example),
    // the memory might be not available.

    __try {
        if (memcmp(dest_memory, original, size) != 0) {
            return (memcmp(dest_memory, patch, size) == 0);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    // Change the memory to writeable.

    DWORD old_protect;
    if (! VirtualProtect(dest_memory, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }

    // Update it.

    memcpy(dest_memory, patch, size);

    // Restore old access mode.

    VirtualProtect(dest_memory, size, old_protect, NULL);
    return true;
}

/**
 * @brief Backs up existing code at specified address and replaces it.
 */
template<size_t size>
static bool backup_and_patch_code(unsigned char (&backup)[size], const size_t code_address, const unsigned char (&patch)[size])
{
    void * const dest_memory = reinterpret_cast<void *>(code_address);

    // Create backup of the memory.

    __try {
        memcpy(backup, dest_memory, size);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    // Change the memory to writeable.

    DWORD old_protect;
    if (! VirtualProtect(dest_memory, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }

    // Update it.

    memcpy(dest_memory, patch, size);

    // Restore old access mode.

    VirtualProtect(dest_memory, size, old_protect, NULL);
    return true;
}

} // anonymous namespace.

/**
 * @brief Constructor.
 *
 * @param the_hw_layer HW communication layer instance. Will be destroyed
 *        when this object is.
 * @param the_instance Instance of this module for use with the window related api.
 */
DirectDrawEmu::DirectDrawEmu(HWLayer * const the_hw_layer, const HINSTANCE the_instance)
    : hw_layer(the_hw_layer)
    , instance(the_instance)
    , window(0)
    , width(0)
    , height(0)
    , bpp(0)
{
    assert(hw_layer);
}

DirectDrawEmu::~DirectDrawEmu()
{
    delete hw_layer;
}

/**
 * @brief Applies hacks improving game behavior with the wrapper.
 */
void DirectDrawEmu::patch_game(void)
{
    // Nothing to do in some helper processes.

    if (is_inside_sfad3d()) {
        logKA(MSG_INFORM, 0, "Running in sfad3d.exe.");
        return;
    }

    if (is_inside_launcher()) {
        logKA(MSG_INFORM, 0, "Running in KALaunch.exe.");
        return;
    }
    if (is_inside_kamovies()) {
        logKA(MSG_INFORM, 0, "Running in kamovies.exe.");
        return;
    }

    // Redirect GetTickCount to timeGetTime() to improve precision.

    if (! is_option_enabled("D3DEMU_NO_TIME_REDIRECT")) {

        // Locate relevant functions. Note that we are using other functions
        // from corresponding modules so the modules will be already loaded
        // and those functions are basic ones.

        const DWORD get_tick_count_proc = reinterpret_cast<DWORD>(GetProcAddress(GetModuleHandleA("Kernel32.dll"), "GetTickCount"));
        const DWORD time_get_time_proc = reinterpret_cast<DWORD>(GetProcAddress(GetModuleHandleA("Winmm.dll"), "timeGetTime"));

        // Update the new code to point to proper address.

        DWORD * const jump_offset = reinterpret_cast<DWORD *>(GET_TICK_COUNT_NEW + 1);
        const DWORD jump_target = time_get_time_proc;
        const DWORD jump_base = get_tick_count_proc + 5;
        *jump_offset = jump_target - jump_base;

        if (backup_and_patch_code(get_tick_count_backup, get_tick_count_proc, GET_TICK_COUNT_NEW)) {
            get_tick_count_patched = true;
            logKA(MSG_INFORM, 0, "GetTickCount redirected to timeGetTime - use D3DEMU_NO_TIME_REDIRECT to disable redirect");
        }
        else{
            logKA(MSG_ERROR, 0, "Unable to redirect GetTickCount");
        }

        // Request maximal possible precision.

        timeBeginPeriod(1);
    }

    // Shading patch.

    const bool is_kaai = is_inside_kaai();
    bool suceeded;
    if (! is_kaai) {
        logKA(MSG_INFORM, 0, "Running in ka.exe.");
        suceeded =
            patch_code(SHADE_MODE_HACK_1_ADDRESS, SHADE_MODE_HACK_1_OLD_KA, SHADE_MODE_HACK_1_NEW_KA) &&
            patch_code(SHADE_MODE_HACK_2_ADDRESS, SHADE_MODE_HACK_2_OLD_KA, SHADE_MODE_HACK_2_NEW_KA)
        ;
    }
    else {
        logKA(MSG_INFORM, 0, "Running in kaai.exe.");
        suceeded =
            patch_code(SHADE_MODE_HACK_1_ADDRESS, SHADE_MODE_HACK_1_OLD_KAAI, SHADE_MODE_HACK_1_NEW_KAAI) &&
            patch_code(SHADE_MODE_HACK_2_ADDRESS, SHADE_MODE_HACK_2_OLD_KAAI, SHADE_MODE_HACK_2_NEW_KAAI)
        ;
    }

    if (suceeded) {
        logKA(MSG_INFORM, 0, "Shading mode patch applied.");
    }
    else {
        logKA(MSG_ERROR, 0, "Unable to apply shading mode patch.");
        unpatch_game();
    }

    // Alpha channel hack.

    if (is_option_enabled("D3DEMU_FORCE_BLACK_ALPHA")) {
        if (patch_code(ALLOW_BLACK_ALPHA_HACK_ADDRESS, ALLOW_BLACK_ALPHA_HACK_OLD, ALLOW_BLACK_ALPHA_HACK_NEW)) {
            logKA(MSG_INFORM, 0, "Black alpha patch applied.");
        }
        else {
            logKA(MSG_ERROR, 0, "Unable to apply black alpha patch.");
        }
    }
    else {
        logKA(MSG_INFORM, 0, "Default alpha handling mode - use D3DEMU_FORCE_BLACK_ALPHA to allow black alpha channel from pcx files.");
    }

    // Should we enable the KA logKA redirection?

    if (! is_option_enabled("D3DEMU_KA_LOG")) {
        logKA(MSG_INFORM, 0, "KA logKA redirection is disabled - use D3DEMU_KA_LOG to enable it");
        return;
    }

    // Set proper address to the byte buffers.

    {
        const DWORD jump_target = reinterpret_cast<DWORD>(redirected_log);
        DWORD * const jump_offset = reinterpret_cast<DWORD *>(LOG_FUNCTION_HACK_1_NEW + 1);
        const DWORD jump_base = LOG_FUNCTION_HACK_1_ADDRESS + 5;
        *jump_offset = jump_target - jump_base;
    }

    {
        const DWORD jump_target = reinterpret_cast<DWORD>(redirected_log_no_level);
        DWORD * const jump_offset = reinterpret_cast<DWORD *>(LOG_FUNCTION_HACK_2_NEW + 1);
        const DWORD jump_base = LOG_FUNCTION_HACK_2_ADDRESS + 5;
        *jump_offset = jump_target - jump_base;
    }

    // Patch it.

    if (
        patch_code(LOG_FUNCTION_HACK_1_ADDRESS, LOG_FUNCTION_HACK_OLD, LOG_FUNCTION_HACK_1_NEW) &&
        patch_code(LOG_FUNCTION_HACK_2_ADDRESS, LOG_FUNCTION_HACK_OLD, LOG_FUNCTION_HACK_2_NEW)
    ) {
        logKA(MSG_INFORM, 0, "KA logKA enabled.");
    }
    else {
        logKA(MSG_ERROR, 0, "Unable to enable KA logKA.");
    }
}

/**
 * @brief Removes hacks installed by patch_game().
 */
void DirectDrawEmu::unpatch_game(void)
{
    // As the patching function checks for expected content, it should do nothing if
    // the path was not applied.

    patch_code(LOG_FUNCTION_HACK_1_ADDRESS, LOG_FUNCTION_HACK_1_NEW, LOG_FUNCTION_HACK_OLD);
    patch_code(LOG_FUNCTION_HACK_2_ADDRESS, LOG_FUNCTION_HACK_2_NEW, LOG_FUNCTION_HACK_OLD);
    patch_code(SHADE_MODE_HACK_1_ADDRESS, SHADE_MODE_HACK_1_NEW_KA, SHADE_MODE_HACK_1_OLD_KA);
    patch_code(SHADE_MODE_HACK_1_ADDRESS, SHADE_MODE_HACK_1_NEW_KAAI, SHADE_MODE_HACK_1_OLD_KAAI);
    patch_code(SHADE_MODE_HACK_2_ADDRESS, SHADE_MODE_HACK_2_NEW_KA, SHADE_MODE_HACK_2_OLD_KA);
    patch_code(SHADE_MODE_HACK_2_ADDRESS, SHADE_MODE_HACK_2_NEW_KAAI, SHADE_MODE_HACK_2_OLD_KAAI);
    patch_code(ALLOW_BLACK_ALPHA_HACK_ADDRESS, ALLOW_BLACK_ALPHA_HACK_NEW, ALLOW_BLACK_ALPHA_HACK_OLD);

    if (get_tick_count_patched) {
        get_tick_count_patched = false;
        const DWORD get_tick_count_proc = reinterpret_cast<DWORD>(GetProcAddress(GetModuleHandleA("Kernel32.dll"), "GetTickCount"));
        patch_code(get_tick_count_proc, GET_TICK_COUNT_NEW, get_tick_count_backup);

        // Disable override.

        timeEndPeriod(1);
    }
}

STDMETHODIMP DirectDrawEmu::EnumDevices(LPD3DENUMDEVICESCALLBACK callback,LPVOID user_arg)
{
    LOG_METHOD();
    CHECK_NOT_NULL(callback);

    // There is only one device. The emulated one.

    GUID guid = EMULATED_DEVICE_GUID;
    callback(&guid, "Emulated device", "Emulated", &device_desc, &device_desc, user_arg);
    return DD_OK;
}

STDMETHODIMP DirectDrawEmu::CreateSurface(LPDDSURFACEDESC desc, LPDIRECTDRAWSURFACE FAR * surface, IUnknown FAR * outer)
{
    LOG_METHOD();
    CHECK_NOT_NULL(desc);
    CHECK_NOT_NULL(surface);
    CHECK_NULL(outer);

    // logKA the structure we got.

    log_structure(MSG_VERBOSE, 1, *desc);

    // Check that we have the necessary info.

    if ((desc->dwFlags & DDSD_CAPS) == 0) {
        logKA(MSG_ERROR, 0, "CreateSurface: Caps bits not present.");
        return DDERR_INVALIDPARAMS;
    }

    // Handling of simple surfaces.

    if ((desc->ddsCaps.dwCaps & DDSCAPS_COMPLEX) == 0) {
        DirectDrawSurfaceEmu * const emu_surface = new DirectDrawSurfaceEmu(*hw_layer, instance);
        emu_surface->initialize(*desc);
        logKA(MSG_VERBOSE, 1, "Created surface %08x", emu_surface);
        *surface = emu_surface;
        return DD_OK;
    }

    // Handling of swap chain. Only some features are supported.

    if ((desc->ddsCaps.dwCaps & DDSCAPS_FLIP) == 0) {
        logKA(MSG_ERROR, 0, "CreateSurface: Creating of flip-less surfaces is not implemented.");
        return DDERR_INVALIDPARAMS;
    }
    if ((desc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) == 0) {
        logKA(MSG_ERROR, 0, "CreateSurface: The flip surface must be a primary one.");
        return DDERR_INVALIDPARAMS;
    }
    if ((desc->dwFlags & DDSD_BACKBUFFERCOUNT) == 0) {
        logKA(MSG_ERROR, 0, "CreateSurface: The flip surface must have back buffer count.");
        return DDERR_INVALIDPARAMS;
    }
    if (desc->dwBackBufferCount != 1) {
        logKA(MSG_ERROR, 0, "CreateSurface: Only one back buffer is supported.");
        return DDERR_INVALIDPARAMS;
    }

    // Fill additional informations to the descriptor.

    DDSURFACEDESC modified_desc = *desc;
    modified_desc.dwFlags |= (DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT);
    modified_desc.dwWidth = width;
    modified_desc.dwHeight = height;
    modified_desc.ddpfPixelFormat.dwSize = sizeof(modified_desc.ddpfPixelFormat);
    modified_desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
    modified_desc.ddpfPixelFormat.dwRGBBitCount = 16;
    modified_desc.ddpfPixelFormat.dwRBitMask = 0x0000F800;
    modified_desc.ddpfPixelFormat.dwGBitMask = 0x000007E0;
    modified_desc.ddpfPixelFormat.dwBBitMask = 0x0000001F;
    modified_desc.ddpfPixelFormat.dwRGBAlphaBitMask = 0x00000000;

    // The launcher does not provide video memory flag.

    modified_desc.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;

    // Front buffer.

    DirectDrawSurfaceEmu * const emu_surface = new DirectDrawSurfaceEmu(*hw_layer, instance);
    modified_desc.ddsCaps.dwCaps |= DDSCAPS_FRONTBUFFER;
    emu_surface->initialize(modified_desc);
    logKA(MSG_VERBOSE, 1, "Created primary surface %08x", emu_surface);

    // Back buffer.

    DirectDrawSurfaceEmu * const back_buffer_surface = new DirectDrawSurfaceEmu(*hw_layer, instance);
    modified_desc.ddsCaps.dwCaps &= ~(DDSCAPS_PRIMARYSURFACE | DDSCAPS_FRONTBUFFER);
    modified_desc.ddsCaps.dwCaps |= DDSCAPS_BACKBUFFER;
    back_buffer_surface->initialize(modified_desc);
    emu_surface->attach_sub_surface(back_buffer_surface, true);
    back_buffer_surface->Release();
    logKA(MSG_VERBOSE, 1, "Created back surface %08x", back_buffer_surface);

    *surface = emu_surface;
    return DD_OK;
}

/**
 * @brief Available display modes.
 *
 * For use in launcher.
 */
STDMETHODIMP DirectDrawEmu::EnumDisplayModes(DWORD flags, LPDDSURFACEDESC reference_desc, LPVOID context, LPDDENUMMODESCALLBACK callback)
{
    LOG_METHOD();
    CHECK_NOT_NULL(callback);

    // Only the flags used by the launcher are supported.

    if (flags != 0) {
        logKA(MSG_ERROR, 0, "EnumDisplayModes: Flags must be zero.");
        return DDERR_INVALIDPARAMS;
    }
    if (reference_desc == NULL) {
        logKA(MSG_ERROR, 0, "EnumDisplayModes: Descriptor must be provided.");
        return DDERR_INVALIDPARAMS;
    }
    CHECK_STRUCTURE(reference_desc, DDSURFACEDESC);

    // Find the real modes.

    DisplayModeList modes;
    if (! hw_layer->get_display_modes(modes)) {
        return DDERR_GENERIC;
    }

    std::set<unsigned __int64> reported_resolutions;

    // Filter them as necessary.

    DisplayModeList::const_iterator it;
    for (it = modes.begin(); it != modes.end(); ++it) {

        // Ignore resolutions we already reported with different refresh rate.

        const unsigned __int64 resolution_id =
            (static_cast<unsigned __int64>(it->width) << 32) |
            static_cast<unsigned __int64>(it->height)
        ;
        if (reported_resolutions.find(resolution_id) != reported_resolutions.end()) {
            continue;
        }

        // Prepare descriptor.

        DDSURFACEDESC result;
        memset(&result, 0, sizeof(result));
        result.dwSize = sizeof(result);
        result.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_REFRESHRATE;
        result.dwWidth = it->width;
        result.dwHeight = it->height;
        result.dwRefreshRate = it->refresh_rate;
        result.ddpfPixelFormat.dwSize = sizeof(result.ddpfPixelFormat);
        result.ddpfPixelFormat.dwFlags = DDPF_RGB;
        result.ddpfPixelFormat.dwRGBBitCount = 16;
        result.ddpfPixelFormat.dwRBitMask = 0x0000F800;
        result.ddpfPixelFormat.dwGBitMask = 0x000007E0;
        result.ddpfPixelFormat.dwBBitMask = 0x0000001F;
        result.ddpfPixelFormat.dwRGBAlphaBitMask = 0x00000000;

        // Filter formats which do not match.

        if ((reference_desc->dwFlags & DDSD_WIDTH) && (reference_desc->dwWidth != result.dwWidth)) {
            continue;
        }
        if ((reference_desc->dwFlags & DDSD_HEIGHT) && (reference_desc->dwHeight != result.dwHeight)) {
            continue;
        }

        // Report to callback.

        reported_resolutions.insert(resolution_id);
        if (callback(&result, context) != DDENUMRET_OK) {
            break;
        }
    }

    return DD_OK;
}

/**
 * @brief Returns device capabilities.
 */
STDMETHODIMP DirectDrawEmu::GetCaps(DDCAPS * driver, DDCAPS * hel)
{
    LOG_METHOD();

    if (driver) {
        const DWORD size = driver->dwSize;
        if (size < sizeof(DDCAPS_DX5)) {
            return DDERR_INVALIDPARAMS;
        }
        if (size > sizeof(DDCAPS_DX6)) {
            return DDERR_INVALIDPARAMS;
        }
        memcpy(driver, &device_caps, size);
        driver->dwSize = size;
    }

    if (hel) {
        const DWORD size = hel->dwSize;
        if (size < sizeof(DDCAPS_DX5)) {
            return DDERR_INVALIDPARAMS;
        }
        if (size > sizeof(DDCAPS_DX6)) {
            return DDERR_INVALIDPARAMS;
        }
        memcpy(hel, &device_caps, size);
        hel->dwSize = size;
    }
    return DD_OK;
}

/**
 * @brief Restores previous display mode.
 */
STDMETHODIMP DirectDrawEmu::RestoreDisplayMode()
{
    // TODO: Destroy HW surfaces to match the old API behavior.
    // The KA does not depend on that so it is not emulated.

    hw_layer->deinitialize();

    // Disable precision boost.

    timeEndPeriod(1);
    return DD_OK;
}

/**
 * @brief Sets cooperative level to use.
 */
STDMETHODIMP DirectDrawEmu::SetCooperativeLevel(HWND hwnd, DWORD flags)
{
    LOG_METHOD();
    logKA(MSG_VERBOSE, 1, "hwnd: %08x", hwnd);
    if (flags & DDSCL_ALLOWMODEX) {
        logKA(MSG_VERBOSE, 2, "ALLOWMODEX");
    }
    if (flags & DDSCL_ALLOWREBOOT) {
        logKA(MSG_VERBOSE, 2, "ALLOWREBOOT");
    }
    if (flags & DDSCL_EXCLUSIVE) {
        logKA(MSG_VERBOSE, 2, "EXCLUSIVE");
    }
    if (flags & DDSCL_FULLSCREEN) {
        logKA(MSG_VERBOSE, 2, "FULLSCREEN");
    }
    if (flags & DDSCL_NORMAL) {
        logKA(MSG_VERBOSE, 2, "NORMAL");
    }
    if (flags & DDSCL_NOWINDOWCHANGES) {
        logKA(MSG_VERBOSE, 2, "NOWINDOWCHANGES");
    }

    // Mode used by the launcher.

    if (flags == DDSCL_NORMAL) {
        window = NULL;
        return DD_OK;
    }

    // We support only one combination for real operation.

    if ((flags & (DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN)) != (DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN)) {
        logKA(MSG_ERROR, 0, "Only exclusive fullscreen mode is supported");
        window = NULL;
        return DDERR_INVALIDPARAMS;
    }

    window = hwnd;
    return DD_OK;
}

/**
 * @brief Set display mode to use.
 */
STDMETHODIMP DirectDrawEmu::SetDisplayMode(DWORD the_width, DWORD the_height, DWORD the_bpp)
{
    LOG_METHOD();
    logKA(MSG_VERBOSE, 1, "Requested resolution %ux%ux%u", the_width, the_height, the_bpp);
    if (window == NULL) {
        return DDERR_NOEXCLUSIVEMODE;
    }

    // Do not bother to set the mode in the launcher. Doing so only slows down the enumeration.

    if (is_inside_launcher()) {
        return DD_OK;
    }

    // Ensure that we work if some resolution was already set. Happens in the launcher if
    // the launcher is not detected by the preceeding line (e.g. renamed file).

    hw_layer->deinitialize();

    // Activate the layer.

    if (! hw_layer->initialize(window, the_width, the_height)) {
        logKA(MSG_ERROR, 1, "HW init failed");
        return DDERR_GENERIC;
    }

    // Increase precision of the timeGetTime() function.

    timeBeginPeriod(1);

    // Remember the parameters.

    width = the_width;
    height = the_height;
    bpp = the_bpp;
    return DD_OK;
}

/**
 * @brief Creates a material object.
 */
STDMETHODIMP DirectDrawEmu::CreateMaterial(LPDIRECT3DMATERIAL * material, IUnknown * outer)
{
    LOG_METHOD();
    CHECK_NOT_NULL(material);
    CHECK_NULL(outer);

    *material = new Direct3DMaterialEmu();
    logKA(MSG_VERBOSE, 1, "Created material %08x", *material);
    return DD_OK;
}

/**
 * @brief Creates a viewport object.
 */
STDMETHODIMP DirectDrawEmu::CreateViewport(LPDIRECT3DVIEWPORT * viewport, IUnknown * outer)
{
    LOG_METHOD();
    CHECK_NOT_NULL(viewport);
    CHECK_NULL(outer);

    *viewport = new Direct3DViewportEmu(*hw_layer);
    logKA(MSG_VERBOSE, 1, "Created viewport %08x", *viewport);
    return DD_OK;
}

} // namespace emu

// EOF //
