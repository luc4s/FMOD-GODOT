/* HARNESS VALIDATION ONLY. Mock FMOD: reads banks through the registered file callbacks (as FMOD
 * does) and plays a quiet tone through Web Audio via EM_JS (FMOD's web output is also JS-backed).
 * Metering reads the real Web Audio output through an AnalyserNode. Proves nothing about FMOD. */
#include "fmod_mock_api.h"
#include <emscripten.h>
#include <string.h>

struct FMOD_SYSTEM
{
    FMOD_FILE_OPEN_CALLBACK open;
    FMOD_FILE_CLOSE_CALLBACK close;
    FMOD_FILE_READ_CALLBACK read;
};
struct FMOD_STUDIO_SYSTEM
{
    int initialized;
};
struct FMOD_STUDIO_BANK
{
    unsigned int size;
};
struct FMOD_STUDIO_EVENTDESCRIPTION
{
    char path[64];
};
struct FMOD_STUDIO_EVENTINSTANCE
{
    int playing;
};
static struct FMOD_SYSTEM core;
static struct FMOD_STUDIO_SYSTEM studio;
static struct FMOD_STUDIO_BANK banks[8];
static int bank_count;
static struct FMOD_STUDIO_EVENTDESCRIPTION desc = {"event:/mock/tone"};
static struct FMOD_STUDIO_EVENTINSTANCE inst;

// clang-format off: EM_JS bodies are JavaScript.
EM_JS(void, mock_audio_init, (void), {
    const ctx = new AudioContext();
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    const analyser = ctx.createAnalyser();
    gain.gain.value = 0.05;
    osc.connect(gain).connect(analyser).connect(ctx.destination);
    globalThis.__mockFmod = {ctx, osc, analyser, started : false};
});
EM_JS(void, mock_audio_start, (void), {
    const m = globalThis.__mockFmod;
    if (!m.started)
    {
        m.osc.start();
        m.started = true;
    }
});
EM_JS(void, mock_audio_resume, (void), { globalThis.__mockFmod.ctx.resume(); });
EM_JS(void, mock_audio_suspend, (void), { globalThis.__mockFmod.ctx.suspend(); });
EM_JS(float, mock_audio_rms, (void), {
    const m = globalThis.__mockFmod;
    if (!m || m.ctx.state != 'running') return 0;
    const buf = new Float32Array(m.analyser.fftSize);
    m.analyser.getFloatTimeDomainData(buf);
    let s = 0;
    for (const v of buf)
        s += v * v;
    return Math.sqrt(s / buf.length);
});

// clang-format on

const char *FMOD_ErrorString(FMOD_RESULT r)
{
    return r == FMOD_OK ? "No errors." : "mock error";
}
FMOD_RESULT FMOD_Debug_Initialize(FMOD_DEBUG_FLAGS f, FMOD_DEBUG_MODE m, FMOD_DEBUG_CALLBACK c, const char *n)
{
    return FMOD_ERR_UNSUPPORTED;
}
FMOD_RESULT FMOD_System_Create(FMOD_SYSTEM **s, unsigned int v)
{
    *s = &core;
    return FMOD_OK;
}
FMOD_RESULT FMOD_System_SetDSPBufferSize(FMOD_SYSTEM *s, unsigned int l, int n)
{
    return FMOD_OK;
}
FMOD_RESULT FMOD_System_SetFileSystem(FMOD_SYSTEM *s, FMOD_FILE_OPEN_CALLBACK o, FMOD_FILE_CLOSE_CALLBACK c,
                                      FMOD_FILE_READ_CALLBACK r, FMOD_FILE_SEEK_CALLBACK k,
                                      FMOD_FILE_ASYNCREAD_CALLBACK a, FMOD_FILE_ASYNCCANCEL_CALLBACK x, int b)
{
    s->open = o;
    s->close = c;
    s->read = r;
    return FMOD_OK;
}
FMOD_RESULT FMOD_System_GetMasterChannelGroup(FMOD_SYSTEM *s, FMOD_CHANNELGROUP **g)
{
    *g = (FMOD_CHANNELGROUP *)1;
    return FMOD_OK;
}
FMOD_RESULT FMOD_ChannelGroup_GetDSP(FMOD_CHANNELGROUP *g, int i, FMOD_DSP **d)
{
    *d = (FMOD_DSP *)1;
    return FMOD_OK;
}
FMOD_RESULT FMOD_DSP_SetMeteringEnabled(FMOD_DSP *d, FMOD_BOOL i, FMOD_BOOL o)
{
    return FMOD_OK;
}
FMOD_RESULT FMOD_DSP_GetMeteringInfo(FMOD_DSP *d, FMOD_DSP_METERING_INFO *in, FMOD_DSP_METERING_INFO *out)
{
    memset(out, 0, sizeof(*out));
    out->numchannels = 1;
    out->rmslevel[0] = inst.playing ? mock_audio_rms() : 0;
    return FMOD_OK;
}
FMOD_RESULT FMOD_System_MixerSuspend(FMOD_SYSTEM *s)
{
    mock_audio_suspend();
    return FMOD_OK;
}
FMOD_RESULT FMOD_System_MixerResume(FMOD_SYSTEM *s)
{
    mock_audio_resume();
    return FMOD_OK;
}
FMOD_RESULT FMOD_System_GetVersion(FMOD_SYSTEM *s, unsigned int *v, unsigned int *b)
{
    *v = FMOD_VERSION;
    if (b)
        *b = 0;
    return FMOD_OK;
}
FMOD_RESULT FMOD_System_GetOutput(FMOD_SYSTEM *s, FMOD_OUTPUTTYPE *o)
{
    *o = FMOD_OUTPUTTYPE_WEBAUDIO;
    return FMOD_OK;
}
FMOD_RESULT FMOD_System_GetSoftwareFormat(FMOD_SYSTEM *s, int *r, FMOD_SPEAKERMODE *m, int *n)
{
    if (r)
        *r = 48000;
    return FMOD_OK;
}
FMOD_RESULT FMOD_System_GetCPUUsage(FMOD_SYSTEM *s, FMOD_CPU_USAGE *u)
{
    memset(u, 0, sizeof(*u));
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_System_Create(FMOD_STUDIO_SYSTEM **s, unsigned int v)
{
    *s = &studio;
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_System_GetCoreSystem(FMOD_STUDIO_SYSTEM *s, FMOD_SYSTEM **c)
{
    *c = &core;
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_System_Initialize(FMOD_STUDIO_SYSTEM *s, int m, FMOD_STUDIO_INITFLAGS f, FMOD_INITFLAGS c,
                                          void *e)
{
    mock_audio_init();
    s->initialized = 1;
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_System_Update(FMOD_STUDIO_SYSTEM *s)
{
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_System_Release(FMOD_STUDIO_SYSTEM *s)
{
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_System_LoadBankFile(FMOD_STUDIO_SYSTEM *s, const char *path, FMOD_STUDIO_LOAD_BANK_FLAGS f,
                                            FMOD_STUDIO_BANK **out)
{
    unsigned int size = 0, got = 0, total = 0;
    void *h = 0;
    char buf[4096];
    FMOD_RESULT r = core.open(path, &size, &h, 0);
    if (r != FMOD_OK)
        return r;
    do
    {
        got = 0;
        core.read(h, buf, sizeof(buf), &got, 0);
        total += got;
    } while (got == sizeof(buf));
    core.close(h, 0);
    if (total != size || bank_count == 8)
        return FMOD_ERR_INVALID_PARAM;
    banks[bank_count].size = size;
    *out = &banks[bank_count++];
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_System_GetEvent(FMOD_STUDIO_SYSTEM *s, const char *p, FMOD_STUDIO_EVENTDESCRIPTION **d)
{
    *d = 0;
    return FMOD_ERR_EVENT_NOTFOUND;
}
FMOD_RESULT FMOD_Studio_System_GetBankList(FMOD_STUDIO_SYSTEM *s, FMOD_STUDIO_BANK **a, int cap, int *n)
{
    *n = 0;
    for (int i = 0; i < bank_count && i < cap; i++)
        a[(*n)++] = &banks[i];
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_Bank_LoadSampleData(FMOD_STUDIO_BANK *b)
{
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_Bank_GetEventList(FMOD_STUDIO_BANK *b, FMOD_STUDIO_EVENTDESCRIPTION **a, int cap, int *n)
{
    *n = cap > 0;
    if (cap > 0)
        a[0] = &desc;
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_EventDescription_GetPath(FMOD_STUDIO_EVENTDESCRIPTION *d, char *p, int size, int *r)
{
    strncpy(p, d->path, size);
    *r = strlen(d->path) + 1;
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_EventDescription_CreateInstance(FMOD_STUDIO_EVENTDESCRIPTION *d, FMOD_STUDIO_EVENTINSTANCE **i)
{
    *i = &inst;
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_EventInstance_Start(FMOD_STUDIO_EVENTINSTANCE *i)
{
    mock_audio_start();
    i->playing = 1;
    return FMOD_OK;
}
FMOD_RESULT FMOD_Studio_EventInstance_GetPlaybackState(FMOD_STUDIO_EVENTINSTANCE *i, FMOD_STUDIO_PLAYBACK_STATE *s)
{
    *s = i->playing ? FMOD_STUDIO_PLAYBACK_PLAYING : FMOD_STUDIO_PLAYBACK_STOPPED;
    return FMOD_OK;
}
