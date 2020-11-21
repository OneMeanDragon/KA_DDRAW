#ifndef CONFIG_H
#define CONFIG_H

namespace emu {

// Configuration queries. Some queries are optimized for repeated queries,
// some are not. Check documentation of respective functions.

bool is_option_enabled(const char * const string);
bool is_cpu_starfield_enabled(void);
bool is_30fps_ui_enabled(void);
bool is_log_flush_enabled(void);
bool is_composition_compare_enabled(void);
bool is_hw_color_conversion_enabled(void);
bool is_surface_cache_enabled(void);
size_t get_anisotropy_level(void);
size_t get_msaa_quality_level(void);

bool is_inside_sfad3d(void);
bool is_inside_launcher(void);
bool is_inside_kaai(void);
bool is_inside_kamovies(void);

} // namespace emu

#endif // CONFIG_H

// EOF //
