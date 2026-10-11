#include "fmod_web_probe.h"

#include <cmath>
#include <fmod.h>
#include <fmod_errors.h>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#ifdef WEB_ENABLED
#include <emscripten/em_asm.h>
#endif

using namespace godot;

namespace FmodWebPhase0
{
namespace
{
FMOD_RESULT F_CALL open_callback(const char *p_name, unsigned int *p_filesize, void **p_handle, void *)
{
    Ref<FileAccess> file = FileAccess::open(String::utf8(p_name), FileAccess::READ);
    if (file.is_null())
    {
        return FMOD_ERR_FILE_NOTFOUND;
    }
    file->reference();
    *p_handle = file.ptr();
    *p_filesize = static_cast<unsigned int>(file->get_length());
    return FMOD_OK;
}

FMOD_RESULT F_CALL close_callback(void *p_handle, void *)
{
    FileAccess *file = reinterpret_cast<FileAccess *>(p_handle);
    file->close();
    if (file->unreference())
    {
        memdelete(file);
    }
    return FMOD_OK;
}

FMOD_RESULT F_CALL read_callback(void *p_handle, void *p_buffer, unsigned int p_size, unsigned int *p_read, void *)
{
    FileAccess *file = reinterpret_cast<FileAccess *>(p_handle);
    *p_read = static_cast<unsigned int>(file->get_buffer(reinterpret_cast<uint8_t *>(p_buffer), p_size));
    return *p_read < p_size ? FMOD_ERR_FILE_EOF : FMOD_OK;
}

FMOD_RESULT F_CALL seek_callback(void *p_handle, unsigned int p_pos, void *)
{
    reinterpret_cast<FileAccess *>(p_handle)->seek(p_pos);
    return FMOD_OK;
}

const char *output_name(FMOD_OUTPUTTYPE p_type)
{
    switch (p_type)
    {
    case FMOD_OUTPUTTYPE_AUTODETECT:
        return "AUTODETECT";
    case FMOD_OUTPUTTYPE_NOSOUND:
        return "NOSOUND";
    case FMOD_OUTPUTTYPE_WEBAUDIO:
        return "WEBAUDIO";
    case FMOD_OUTPUTTYPE_AUDIOWORKLET:
        return "AUDIOWORKLET";
    default:
        return "OTHER";
    }
}
} // namespace

#ifdef WEB_ENABLED
// FMOD's EM_ASM output code does Module["cwrap"]('FMOD_JS_MixFunction', ...), and cwrap resolves
// Module["_FMOD_JS_MixFunction"]. Exports of a dynamically loaded side module are merged into the
// loader's `wasmImports` table, not onto Module, so cwrap returns undefined and the mixer callback
// throws. Godot evaluates side-module EM_ASM inside the loader scope, where `wasmImports` is visible.
static int publish_fmod_js_exports()
{
    return EM_ASM_INT({
        // Parenthesized so the preprocessor doesn't split the macro argument on the commas.
        // Only C++-compatible operators (no === / !==): clang-format would split them.
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
#endif

bool FmodWebProbe::check(FMOD_RESULT p_result, const char *p_what)
{
    if (p_result == FMOD_OK)
    {
        return true;
    }
    String msg = vformat("%s failed: %d %s", p_what, (int)p_result, FMOD_ErrorString(p_result));
    status["error"] = msg;
    UtilityFunctions::print("FMODPROBE ERROR ", msg);
    return false;
}

void FmodWebProbe::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_status"), &FmodWebProbe::get_status);
}

void FmodWebProbe::_ready()
{
    status["stage"] = "create";
    if (!check(FMOD_Studio_System_Create(&studio, FMOD_VERSION), "Studio_System_Create"))
        return;
    if (!check(FMOD_Studio_System_GetCoreSystem(studio, &core), "GetCoreSystem"))
        return;
    if (!check(FMOD_System_SetFileSystem(core, open_callback, close_callback, read_callback, seek_callback, nullptr,
                                         nullptr, 2048),
               "SetFileSystem"))
        return;

#ifdef WEB_ENABLED
    status["js_exports_missing"] = publish_fmod_js_exports();
#endif

    status["stage"] = "initialize";
    if (!check(FMOD_Studio_System_Initialize(studio, 512, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr),
               "Studio_System_Initialize"))
        return;

    FMOD_OUTPUTTYPE output = FMOD_OUTPUTTYPE_AUTODETECT;
    FMOD_System_GetOutput(core, &output);
    status["output"] = output_name(output);
    unsigned int version = 0, buildnumber = 0;
    FMOD_System_GetVersion(core, &version, &buildnumber);
    status["fmod_version"] = vformat("%x", version);

    status["stage"] = "load_banks";
    const char *banks[] = {"res://banks/Master.bank", "res://banks/Master.strings.bank", "res://banks/SFX.bank"};
    for (const char *bank_path : banks)
    {
        FMOD_STUDIO_BANK *bank = nullptr;
        if (!check(FMOD_Studio_System_LoadBankFile(studio, bank_path, FMOD_STUDIO_LOAD_BANK_NORMAL, &bank), bank_path))
            return;
    }

    status["stage"] = "play";
    FMOD_STUDIO_EVENTDESCRIPTION *desc = nullptr;
    if (!check(FMOD_Studio_System_GetEvent(studio, "event:/Ambience/Country", &desc), "GetEvent"))
        return;
    if (!check(FMOD_Studio_EventDescription_CreateInstance(desc, &instance), "CreateInstance"))
        return;
    if (!check(FMOD_Studio_EventInstance_Start(instance), "EventInstance_Start"))
        return;

    // Meter the master channel group so we can tell if the mixer is actually producing audio.
    FMOD_CHANNELGROUP *master = nullptr;
    if (check(FMOD_System_GetMasterChannelGroup(core, &master), "GetMasterChannelGroup") &&
        check(FMOD_ChannelGroup_GetDSP(master, FMOD_CHANNELCONTROL_DSP_HEAD, &master_dsp), "ChannelGroup_GetDSP"))
    {
        check(FMOD_DSP_SetMeteringEnabled(master_dsp, false, true), "DSP_SetMeteringEnabled");
    }

    status["stage"] = "running";
    UtilityFunctions::print("FMODPROBE READY output=", status["output"], " version=", status["fmod_version"],
                            " js_exports_missing=", status.get("js_exports_missing", -1));
}

void FmodWebProbe::_process(double p_delta)
{
    if (!studio || String(status.get("stage", "")) != "running")
        return;

    check(FMOD_Studio_System_Update(studio), "Studio_System_Update");
    updates++;
    elapsed += p_delta;

    FMOD_STUDIO_PLAYBACK_STATE state = FMOD_STUDIO_PLAYBACK_STOPPED;
    int position = 0;
    FMOD_Studio_EventInstance_GetPlaybackState(instance, &state);
    FMOD_Studio_EventInstance_GetTimelinePosition(instance, &position);

    float rms = 0.0f;
    if (master_dsp)
    {
        FMOD_DSP_METERING_INFO out = {};
        if (FMOD_DSP_GetMeteringInfo(master_dsp, nullptr, &out) == FMOD_OK)
        {
            for (int i = 0; i < out.numchannels; i++)
                rms = std::fmax(rms, out.rmslevel[i]);
        }
    }
    peak_rms = std::fmax(peak_rms, rms);

    FMOD_STUDIO_CPU_USAGE cpu_studio = {};
    FMOD_CPU_USAGE cpu_core = {};
    FMOD_Studio_System_GetCPUUsage(studio, &cpu_studio, &cpu_core);

    status["updates"] = (int64_t)updates;
    status["playback_state"] = (int)state;
    status["timeline_ms"] = position;
    status["rms"] = rms;
    status["peak_rms"] = peak_rms;
    status["cpu_mixer"] = cpu_core.dsp;

    if (elapsed >= next_report)
    {
        next_report = elapsed + 1.0;
        UtilityFunctions::print(vformat("FMODPROBE STATUS t=%.1f updates=%d state=%d timeline_ms=%d rms=%.4f "
                                        "peak_rms=%.4f dsp_cpu=%.2f",
                                        elapsed, (int64_t)updates, (int)state, position, rms, peak_rms, cpu_core.dsp));
    }
}

void FmodWebProbe::_exit_tree()
{
    if (studio)
    {
        FMOD_Studio_System_Release(studio);
        studio = nullptr;
    }
}

Dictionary FmodWebProbe::get_status() const
{
    return status;
}
} // namespace FmodWebPhase0
