#include "config.h"
#include <stdlib.h>
#include "log.h"
#include "common.h"
#include <windows.h>

namespace emu {

namespace {

/**
 * @brief Indication that feature is disabled or enabled.
 *
 * Negative value means that the value was not detected yet.
 */
int cpu_starfield_disabled = -1;
int ui_30fps_enabled = -1;
int log_flush_enabled = -1;
int composition_compare_enabled = -1;
int hw_color_conversion_enabled = -1;
int hw_surface_cache_enabled = -1;
size_t msaa_quality_level = static_cast<size_t>(-1);
int inside_sfad3d = -1;

} // anonymous namespace

/**
 * @brief Checks if specified configuration option is enabled.
 *
 * This is currently done by checking if specified string is name of
 * existing environment variable.
 */
bool is_option_enabled(const char * const string)
{
    return (getenv(string) != NULL);
}

/**
 * @brief Determines if cpu drawn starfield is enabled.
 *
 * Optimized for frequent queries.
 */
bool is_cpu_starfield_enabled(void)
{
    if (cpu_starfield_disabled == -1) {
        cpu_starfield_disabled = is_option_enabled("D3DEMU_NO_CPU_STARFIELD") ? 1 : 0;

        // Report the state.

        if (cpu_starfield_disabled > 0) {
            logKA(MSG_INFORM, 0, "CPU drawn starfield is disabled")
        }
        else {
            logKA(MSG_INFORM, 0, "CPU drawn starfield is enabled - use D3DEMU_NO_CPU_STARFIELD to disable it")
        }
    }
    return (cpu_starfield_disabled == 0);
}

/**
 * @brief Should we allow higher refresh rate in the ui?
 *
 * Optimized for frequent queries.
 */
bool is_30fps_ui_enabled(void)
{
    if (ui_30fps_enabled == -1) {
        ui_30fps_enabled = is_option_enabled("D3DEMU_UI_30FPS") ? 1 : 0;

        // Report the state.

        if (ui_30fps_enabled > 0) {
            logKA(MSG_INFORM, 0, "30FPS UI mode is enabled.")
        }
        else {
            logKA(MSG_INFORM, 0, "30FPS UI mode is disabled - use D3DEMU_UI_30FPS to enable it.")
        }
    }
    return (ui_30fps_enabled > 0);
}

/**
 * @brief Indicates if logKA should be flushed after each entry.
 *
 * Optimized for frequent queries.
 */
bool is_log_flush_enabled(void)
{
    if (log_flush_enabled == -1) {
        log_flush_enabled = is_option_enabled("D3DEMU_LOG_FLUSH") ? 1 : 0;

        // Report the state.

        if (log_flush_enabled > 0) {
            logKA(MSG_INFORM, 0, "logKA flushing enabled")
        }
        else {
            logKA(MSG_INFORM, 0, "logKA flushing disabled - use D3DEMU_LOG_FLUSH to enable it.")
        }
    }
    return (log_flush_enabled > 0);
}

/**
 * @brief Indicates if the composition surface should be compared on the cpu and
 * ignored if it is in the default black state.
 *
 * Optimized for frequent queries.
 */
bool is_composition_compare_enabled(void)
{
    if (composition_compare_enabled == -1) {
        composition_compare_enabled = is_option_enabled("D3DEMU_NO_COMPOSITION_COMPARE") ? 0 : 1;

        // Report the state.

        if (composition_compare_enabled > 0) {
            logKA(MSG_INFORM, 0, "Composition compare enabled - use D3DEMU_NO_COMPOSITION_COMPARE to disable it.")
        }
        else {
            logKA(MSG_INFORM, 0, "Composition compare disabled")
        }
    }
    return (composition_compare_enabled > 0);
}

/**
 * @brief Indicates if the 32<->16 conversion for render targets should be done on the GPU.
 *
 * Optimized for frequent queries.
 */
bool is_hw_color_conversion_enabled(void)
{
    if (hw_color_conversion_enabled == -1) {
        hw_color_conversion_enabled = is_option_enabled("D3DEMU_NO_HW_COLOR_CONVERSION") ? 0 : 1;

        // Report the state.

        if (hw_color_conversion_enabled > 0) {
            logKA(MSG_INFORM, 0, "HW 16<->32 conversion enabled - use D3DEMU_NO_HW_COLOR_CONVERSION to disable it.")
        }
        else {
            logKA(MSG_INFORM, 0, "HW 16<->32 conversion disabled")
        }
    }
    return (hw_color_conversion_enabled > 0);
}

/**
 * @brief Indicates if we should cache HW surfaces for reuse.
 *
 * Optimized for frequent queries.
 */
bool is_surface_cache_enabled(void)
{
    if (hw_surface_cache_enabled == -1) {
        hw_surface_cache_enabled = is_option_enabled("D3DEMU_NO_HW_SURFACE_CACHE") ? 0 : 1;

        // Report the state.

        if (hw_surface_cache_enabled > 0) {
            logKA(MSG_INFORM, 0, "HW surface cache enabled - use D3DEMU_NO_HW_SURFACE_CACHE to disable it.")
        }
        else {
            logKA(MSG_INFORM, 0, "HW surface cache disabled")
        }
    }
    return (hw_surface_cache_enabled > 0);
}

/**
 * @brief Detects desired level of anisotropic filtering.
 *
 * Returns 0 if configuration does not exist or is not valid.
 * If "max" is specified, returns INT_MAX.
 * Otherwise 1-based anisotropy level (1 .. minimal anisotropy).
 */
size_t get_anisotropy_level(void)
{
    // Is there any overridde?

    const char * const env_value = getenv("D3DEMU_ANISOTROPY");
    if (env_value == NULL) {
        return 0;
    }

    // Special value corresponding to hw maximum.

    if (strcmp(env_value, "max") == 0) {
        return INT_MAX;
    }

    // Pase the value.

    const int value = atoi(env_value);
    if (value < 1) {
        return 0;
    }

    return static_cast<size_t>(value);
}

/**
 * @brief Returns desired quality of MSAA.
 *
 * Returns 0 if configuration does not exist or is not valid.
 * If "max" is specified, returns INT_MAX.
 * Otherwise 1-based quality level (1 .. minimal quality).
 *
 * Optimized for frequent queries.
 */
size_t get_msaa_quality_level(void)
{
    if (msaa_quality_level != static_cast<size_t>(-1)) {
        return msaa_quality_level;
    }

    // Is there any overridde?

    const char * const env_value = getenv("D3DEMU_MSAA_QUALITY");
    if (env_value == NULL) {
        msaa_quality_level = 0;
        return msaa_quality_level;
    }

    // Special value corresponding to hw maximum.

    if (strcmp(env_value, "max") == 0) {
        msaa_quality_level = INT_MAX;
        return msaa_quality_level;
    }

    // Pase the value.

    const int value = atoi(env_value);
    if (value < 1) {
        msaa_quality_level = 0;
        return msaa_quality_level;
    }

    msaa_quality_level = static_cast<size_t>(value);
    return msaa_quality_level;
}

/**
 * @brief Detects if we are called from specified application.
 */
bool is_inside_app(const char * const exe_name)
{
    char buffer[MAX_PATH + 1];
    memset(buffer, 0, sizeof(buffer));

    // Retrieve name of the module.

    const size_t length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if ((length == 0) || (length >= MAX_PATH)) {
        return false;
    }

    // Ensure that we have one case to compare against

    _strupr(buffer);

    // Look for name of the launcher.

    return (strstr(buffer, exe_name) != NULL);
}

/**
 * @brief Detects if we are called from inside of the SFAD3D exe.
 *
 * If the function is not sure, it will return false.
 *
 * Optimized for frequent queries.
 */
bool is_inside_sfad3d(void)
{
    if (inside_sfad3d == -1) {
        inside_sfad3d = is_inside_app("SFAD3D.EXE") ? 1 : 0;
    }
    return (inside_sfad3d != 0);
}

/**
 * @brief Detects if we are called from inside of the launcher.
 *
 * If the function is not sure, it will return false.
 */
bool is_inside_launcher(void)
{
    return is_inside_app("KALAUNCH.EXE");
}

/**
 * @brief Detects if we are called from inside of the kaai.
 *
 * If the function is not sure, it will return false.
 */
bool is_inside_kaai(void)
{
    return is_inside_app("KAAI.EXE");
}

/**
 * @brief Detects if we are called from inside of the kamovies.
 *
 * If the function is not sure, it will return false.
 */
bool is_inside_kamovies(void)
{
    return is_inside_app("KAMOVIES.EXE");
}

} // namespace emu

// EOF //
