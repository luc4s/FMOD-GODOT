// Phase 0 web feasibility spike: the smallest GDExtension that drives FMOD Studio
// on the web. Throwaway code, intentionally independent of the main plugin.
#include <fmod.h>
#include <fmod_errors.h>
#include <fmod_studio.h>

#include <gdextension_interface.h>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace
{
String fmod_error(FMOD_RESULT p_result)
{
    return p_result == FMOD_OK ? String() : vformat("%d: %s", p_result, FMOD_ErrorString(p_result));
}

FMOD_RESULT F_CALL debug_callback(FMOD_DEBUG_FLAGS, const char *p_file, int p_line, const char *p_func,
                                  const char *p_message)
{
    UtilityFunctions::print("FMOD: ", p_file, ":", p_line, " ", p_func, " ", String(p_message).strip_edges());
    return FMOD_OK;
}

// Same approach as the main plugin: route FMOD file I/O through Godot's FileAccess so res:// works.
FMOD_RESULT F_CALL open_callback(const char *p_name, unsigned int *p_filesize, void **p_handle, void *)
{
    Ref<FileAccess> file = FileAccess::open(String(p_name), FileAccess::READ);
    if (file.is_null())
    {
        return FMOD_ERR_FILE_NOTFOUND;
    }
    file->reference();
    *p_filesize = file->get_length();
    *p_handle = file.ptr();
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
    *p_read = file->get_buffer(reinterpret_cast<uint8_t *>(p_buffer), p_size);
    return *p_read < p_size ? FMOD_ERR_FILE_EOF : FMOD_OK;
}
FMOD_RESULT F_CALL seek_callback(void *p_handle, unsigned int p_pos, void *)
{
    reinterpret_cast<FileAccess *>(p_handle)->seek(p_pos);
    return FMOD_OK;
}
} // namespace

class FmodWebSpike : public Node
{
    GDCLASS(FmodWebSpike, Node)

    FMOD_STUDIO_SYSTEM *studio = nullptr;
    FMOD_SYSTEM *core = nullptr;
    FMOD_STUDIO_EVENTINSTANCE *instance = nullptr;
    FMOD_DSP *master_head = nullptr;
    String playing_path;
    int updates = 0;

  protected:
    static void _bind_methods()
    {
        ClassDB::bind_method(D_METHOD("init_fmod", "dsp_buffer_length", "dsp_buffer_count"), &FmodWebSpike::init_fmod);
        ClassDB::bind_method(D_METHOD("load_bank", "path"), &FmodWebSpike::load_bank);
        ClassDB::bind_method(D_METHOD("play", "event_path"), &FmodWebSpike::play);
        ClassDB::bind_method(D_METHOD("mixer_suspend"), &FmodWebSpike::mixer_suspend);
        ClassDB::bind_method(D_METHOD("mixer_resume"), &FmodWebSpike::mixer_resume);
        ClassDB::bind_method(D_METHOD("update"), &FmodWebSpike::update);
        ClassDB::bind_method(D_METHOD("get_status"), &FmodWebSpike::get_status);
    }

  public:
    ~FmodWebSpike() override
    {
        if (studio)
        {
            FMOD_Studio_System_Release(studio);
        }
    }

    String init_fmod(int p_dsp_buffer_length, int p_dsp_buffer_count)
    {
        // Fails with FMOD_ERR_UNSUPPORTED on non-logging libraries; that is expected.
        FMOD_Debug_Initialize(FMOD_DEBUG_LEVEL_WARNING, FMOD_DEBUG_MODE_CALLBACK, debug_callback, nullptr);

        FMOD_RESULT r = FMOD_Studio_System_Create(&studio, FMOD_VERSION);
        if (r != FMOD_OK)
        {
            return "Studio_System_Create " + fmod_error(r);
        }
        FMOD_Studio_System_GetCoreSystem(studio, &core);
        if (p_dsp_buffer_length > 0)
        {
            r = FMOD_System_SetDSPBufferSize(core, p_dsp_buffer_length, p_dsp_buffer_count);
            if (r != FMOD_OK)
            {
                return "SetDSPBufferSize " + fmod_error(r);
            }
        }
        r = FMOD_System_SetFileSystem(core, open_callback, close_callback, read_callback, seek_callback, nullptr,
                                      nullptr, 2048);
        if (r != FMOD_OK)
        {
            return "SetFileSystem " + fmod_error(r);
        }
        r = FMOD_Studio_System_Initialize(studio, 256, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, nullptr);
        if (r != FMOD_OK)
        {
            return "Studio_System_Initialize " + fmod_error(r);
        }
        FMOD_CHANNELGROUP *master = nullptr;
        FMOD_System_GetMasterChannelGroup(core, &master);
        FMOD_ChannelGroup_GetDSP(master, FMOD_CHANNELCONTROL_DSP_HEAD, &master_head);
        FMOD_DSP_SetMeteringEnabled(master_head, false, true);
        return String();
    }

    String load_bank(const String &p_path)
    {
        FMOD_STUDIO_BANK *bank = nullptr;
        String err = fmod_error(
            FMOD_Studio_System_LoadBankFile(studio, p_path.utf8().get_data(), FMOD_STUDIO_LOAD_BANK_NORMAL, &bank));
        if (err.is_empty())
        {
            FMOD_Studio_Bank_LoadSampleData(bank);
        }
        return err;
    }

    // Plays p_event_path, or the first event found in any loaded bank if it is empty or missing.
    String play(const String &p_event_path)
    {
        FMOD_STUDIO_EVENTDESCRIPTION *desc = nullptr;
        if (!p_event_path.is_empty())
        {
            FMOD_Studio_System_GetEvent(studio, p_event_path.utf8().get_data(), &desc);
        }
        if (!desc)
        {
            FMOD_STUDIO_BANK *banks[64];
            int bank_count = 0;
            FMOD_Studio_System_GetBankList(studio, banks, 64, &bank_count);
            for (int i = 0; i < bank_count && !desc; i++)
            {
                int event_count = 0;
                FMOD_Studio_Bank_GetEventList(banks[i], &desc, 1, &event_count);
                if (event_count == 0)
                {
                    desc = nullptr;
                }
            }
        }
        if (!desc)
        {
            return "no event found in loaded banks";
        }
        char path[512] = {};
        int retrieved = 0;
        FMOD_Studio_EventDescription_GetPath(desc, path, sizeof(path), &retrieved);
        playing_path = String(path);
        FMOD_RESULT r = FMOD_Studio_EventDescription_CreateInstance(desc, &instance);
        if (r != FMOD_OK)
        {
            return "CreateInstance " + fmod_error(r);
        }
        return fmod_error(FMOD_Studio_EventInstance_Start(instance));
    }

    String mixer_suspend()
    {
        return fmod_error(FMOD_System_MixerSuspend(core));
    }
    String mixer_resume()
    {
        return fmod_error(FMOD_System_MixerResume(core));
    }

    // Called from GDScript every frame: the web build has no threads to run this on.
    String update()
    {
        updates++;
        return fmod_error(FMOD_Studio_System_Update(studio));
    }

    Dictionary get_status() const
    {
        Dictionary d;
        unsigned int version = 0;
        FMOD_System_GetVersion(core, &version, nullptr);
        d["version"] = vformat("%x", version);
        d["updates"] = updates;
        d["event"] = playing_path;
        FMOD_STUDIO_PLAYBACK_STATE state = FMOD_STUDIO_PLAYBACK_STOPPED;
        if (instance)
        {
            FMOD_Studio_EventInstance_GetPlaybackState(instance, &state);
        }
        d["playback_state"] = int(state);
        float rms = 0.0f;
        if (master_head)
        {
            FMOD_DSP_METERING_INFO info = {};
            if (FMOD_DSP_GetMeteringInfo(master_head, nullptr, &info) == FMOD_OK)
            {
                for (int i = 0; i < info.numchannels; i++)
                {
                    rms = MAX(rms, info.rmslevel[i]);
                }
            }
        }
        d["master_rms"] = rms;
        FMOD_OUTPUTTYPE output = FMOD_OUTPUTTYPE_AUTODETECT;
        FMOD_System_GetOutput(core, &output);
        d["output_type"] = int(output);
        int sample_rate = 0;
        FMOD_System_GetSoftwareFormat(core, &sample_rate, nullptr, nullptr);
        d["sample_rate"] = sample_rate;
        FMOD_CPU_USAGE cpu = {};
        FMOD_System_GetCPUUsage(core, &cpu);
        d["cpu_dsp"] = cpu.dsp;
        return d;
    }
};

static void initialize(ModuleInitializationLevel p_level)
{
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        GDREGISTER_CLASS(FmodWebSpike);
    }
}
static void uninitialize(ModuleInitializationLevel)
{
}

extern "C" GDExtensionBool GDE_EXPORT fmod_web_spike_init(GDExtensionInterfaceGetProcAddress p_get_proc_address,
                                                          GDExtensionClassLibraryPtr p_library,
                                                          GDExtensionInitialization *r_initialization)
{
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize);
    init_obj.register_terminator(uninitialize);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
