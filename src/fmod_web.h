#pragma once

// Glue between FMOD's HTML5 build and Godot's web runtime. Every function is a no-op on other platforms, so callers
// don't need #ifdefs.
namespace FmodGodot::Web
{
#ifdef WEB_ENABLED
// FMOD's JavaScript looks up its mixer callbacks with Module.cwrap(), which only finds functions published on
// Module. Godot loads the extension as a side module, whose exports are not published there. Must be called before
// the FMOD output is initialized. Returns the number of callbacks that could not be found (0 on success).
int publish_fmod_js_exports();

// FMOD only resumes its AudioContext on click/touchend. Also resume it on the other events browsers accept as a
// user gesture (key presses in particular), using FMOD's own resume handler. The listeners remove themselves once
// FMOD's audio is unlocked.
void install_audio_unlock();
void remove_audio_unlock();

// Messages from FMOD's debug callback that are noise on the web build.
bool is_ignored_log_message(const char *p_message);
#else
inline int publish_fmod_js_exports()
{
    return 0;
}
inline void install_audio_unlock()
{
}
inline void remove_audio_unlock()
{
}
inline bool is_ignored_log_message(const char *p_message)
{
    return false;
}
#endif
} // namespace FmodGodot::Web
