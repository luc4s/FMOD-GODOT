#pragma once

#include <fmod_studio.h>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace FmodWebPhase0
{
// Minimal go/no-go probe: create + initialize the Studio system, load banks
// through Godot FileAccess callbacks (same approach as FmodAudioServer), play
// one event and report what happens. Studio update runs on the main thread
// from _process, which is what a single-threaded web build has to do.
class FmodWebProbe : public godot::Node
{
    GDCLASS(FmodWebProbe, godot::Node)

    FMOD_STUDIO_SYSTEM *studio = nullptr;
    FMOD_SYSTEM *core = nullptr;
    FMOD_STUDIO_EVENTINSTANCE *instance = nullptr;
    FMOD_DSP *master_dsp = nullptr;
    godot::Dictionary status;
    double elapsed = 0.0;
    double next_report = 0.0;
    uint64_t updates = 0;
    float peak_rms = 0.0f;

    bool check(FMOD_RESULT p_result, const char *p_what);

  protected:
    static void _bind_methods();

  public:
    void _ready() override;
    void _process(double p_delta) override;
    void _exit_tree() override;

    godot::Dictionary get_status() const;
};
} // namespace FmodWebPhase0
