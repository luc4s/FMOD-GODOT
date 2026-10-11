#ifdef WEB_ENABLED
#include "fmod_web.h"

#include <cstring>
#include <emscripten/em_asm.h>

// Notes for the JavaScript inside EM_ASM below:
// - Godot evaluates the EM_ASM code of side modules inside its dynamic loader, so `Module` is the engine's module
//   and `wasmImports` holds the exports of loaded side modules (Emscripten internals, re-check on Emscripten updates).
// - Commas outside parentheses would split the macro argument, so array literals are wrapped in parentheses.
// - Only C++-compatible operators (no === or !==), otherwise clang-format splits them.

namespace FmodGodot::Web
{
int publish_fmod_js_exports()
{
    return EM_ASM_INT({
        var names = ([ "FMOD_JS_MixFunction", "FMOD_JS_MixerSlowpathFunction", "FMOD_JS_MixerFastpathFunction" ]);
        var missing = 0;
        for (var i = 0; i < names.length; i++)
        {
            var fn = (typeof wasmImports != "undefined") ? wasmImports[names[i]] : undefined;
            if (typeof fn == "function")
            {
                Module["_" + names[i]] = fn;
            }
            else
            {
                missing++;
            }
        }
        return missing;
    });
}

void install_audio_unlock()
{
    EM_ASM({
        if (Module.fmodGodotAudioUnlock)
        {
            return;
        }
        var target = Module.window ? Module.window : window;
        var events = ([ "keydown", "mousedown", "pointerdown", "pointerup", "touchend" ]);
        var unlock = function(e)
        {
            // FMOD's resume handler only runs once, so only call it for events that can start an AudioContext: not
            // for the pointerdown of a touch, Escape or modifier keys, or synthetic events.
            var active = false;
            if (navigator.userActivation)
            {
                active = navigator.userActivation.isActive;
            }
            else
            {
                active = e.isTrusted && !(e.type == "pointerdown" && e.pointerType != "mouse") &&
                         !(e.type == "pointerup" && e.pointerType == "mouse") &&
                         !(e.type == "keydown" && (e.key == "Escape" || e.key == "Shift" || e.key == "Control" ||
                                                   e.key == "Alt" || e.key == "Meta"));
            }
            if (!active)
            {
                return;
            }
            // Set by FMOD's output when it registers its own click/touchend handlers; they guard on mInputRegistered,
            // create the audio worklet and resume the AudioContext.
            var resume_worklet = Module["OutputAudioWorklet_resumeAudio"];
            if (typeof resume_worklet == "function")
            {
                resume_worklet();
            }
            var resume_webaudio = Module["OutputWebAudio_resumeAudio"];
            if (typeof resume_webaudio == "function")
            {
                resume_webaudio();
            }
            // FMOD clears mInputRegistered once its audio has been unlocked.
            if (!Module.mInputRegistered)
            {
                Module.fmodGodotAudioUnlock.remove();
            }
        };
        var remove = function()
        {
            for (var i = 0; i < events.length; i++)
            {
                target.removeEventListener(events[i], unlock, true);
            }
            delete Module.fmodGodotAudioUnlock;
        };
        for (var i = 0; i < events.length; i++)
        {
            target.addEventListener(events[i], unlock, true);
        }
        Module.fmodGodotAudioUnlock = {remove : remove};
    });
}

void remove_audio_unlock()
{
    EM_ASM({
        if (Module.fmodGodotAudioUnlock)
        {
            Module.fmodGodotAudioUnlock.remove();
        }
    });
}

bool is_ignored_log_message(const char *p_message)
{
    // The single-threaded FMOD logging library reports every critical section it skips, hundreds of times a second.
    return p_message && std::strstr(p_message, "NULL critical section");
}
} // namespace FmodGodot::Web
#endif // WEB_ENABLED
