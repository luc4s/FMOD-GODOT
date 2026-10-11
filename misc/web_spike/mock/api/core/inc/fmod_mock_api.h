/* HARNESS VALIDATION ONLY: a tiny stand-in for the FMOD C API subset used by src/spike.cpp.
 * Signatures mirror FMOD 2.03 so the spike compiles unchanged against the real headers. */
#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
#define F_CALL
#define F_API
#define FMOD_VERSION 0x00020315
    typedef int FMOD_RESULT;
    typedef int FMOD_BOOL;
    typedef unsigned int FMOD_DEBUG_FLAGS, FMOD_INITFLAGS, FMOD_STUDIO_INITFLAGS, FMOD_STUDIO_LOAD_BANK_FLAGS;
    typedef int FMOD_DEBUG_MODE, FMOD_OUTPUTTYPE, FMOD_SPEAKERMODE, FMOD_STUDIO_PLAYBACK_STATE;
    enum
    {
        FMOD_OK = 0,
        FMOD_ERR_FILE_EOF = 13,
        FMOD_ERR_FILE_NOTFOUND = 18,
        FMOD_ERR_INVALID_PARAM = 31,
        FMOD_ERR_UNSUPPORTED = 66,
        FMOD_ERR_EVENT_NOTFOUND = 74
    };
    enum
    {
        FMOD_DEBUG_LEVEL_WARNING = 2,
        FMOD_DEBUG_MODE_CALLBACK = 2
    };
    enum
    {
        FMOD_INIT_NORMAL = 0,
        FMOD_STUDIO_INIT_NORMAL = 0,
        FMOD_STUDIO_LOAD_BANK_NORMAL = 0
    };
    enum
    {
        FMOD_CHANNELCONTROL_DSP_HEAD = -1,
        FMOD_OUTPUTTYPE_AUTODETECT = 0,
        FMOD_OUTPUTTYPE_WEBAUDIO = 12
    };
    enum
    {
        FMOD_STUDIO_PLAYBACK_PLAYING = 0,
        FMOD_STUDIO_PLAYBACK_STOPPED = 2
    };
    typedef struct FMOD_SYSTEM FMOD_SYSTEM;
    typedef struct FMOD_CHANNELGROUP FMOD_CHANNELGROUP;
    typedef struct FMOD_DSP FMOD_DSP;
    typedef struct FMOD_STUDIO_SYSTEM FMOD_STUDIO_SYSTEM;
    typedef struct FMOD_STUDIO_BANK FMOD_STUDIO_BANK;
    typedef struct FMOD_STUDIO_EVENTDESCRIPTION FMOD_STUDIO_EVENTDESCRIPTION;
    typedef struct FMOD_STUDIO_EVENTINSTANCE FMOD_STUDIO_EVENTINSTANCE;
    typedef struct
    {
        int numsamples;
        float peaklevel[32];
        float rmslevel[32];
        short numchannels;
    } FMOD_DSP_METERING_INFO;
    typedef struct
    {
        float dsp, stream, geometry, update, convolution1, convolution2;
    } FMOD_CPU_USAGE;
    typedef FMOD_RESULT (*FMOD_DEBUG_CALLBACK)(FMOD_DEBUG_FLAGS, const char *, int, const char *, const char *);
    typedef FMOD_RESULT (*FMOD_FILE_OPEN_CALLBACK)(const char *, unsigned int *, void **, void *);
    typedef FMOD_RESULT (*FMOD_FILE_CLOSE_CALLBACK)(void *, void *);
    typedef FMOD_RESULT (*FMOD_FILE_READ_CALLBACK)(void *, void *, unsigned int, unsigned int *, void *);
    typedef FMOD_RESULT (*FMOD_FILE_SEEK_CALLBACK)(void *, unsigned int, void *);
    typedef void *FMOD_FILE_ASYNCREAD_CALLBACK, *FMOD_FILE_ASYNCCANCEL_CALLBACK;

    const char *FMOD_ErrorString(FMOD_RESULT);
    FMOD_RESULT FMOD_Debug_Initialize(FMOD_DEBUG_FLAGS, FMOD_DEBUG_MODE, FMOD_DEBUG_CALLBACK, const char *);
    FMOD_RESULT FMOD_System_Create(FMOD_SYSTEM **, unsigned int);
    FMOD_RESULT FMOD_System_SetDSPBufferSize(FMOD_SYSTEM *, unsigned int, int);
    FMOD_RESULT FMOD_System_SetFileSystem(FMOD_SYSTEM *, FMOD_FILE_OPEN_CALLBACK, FMOD_FILE_CLOSE_CALLBACK,
                                          FMOD_FILE_READ_CALLBACK, FMOD_FILE_SEEK_CALLBACK,
                                          FMOD_FILE_ASYNCREAD_CALLBACK, FMOD_FILE_ASYNCCANCEL_CALLBACK, int);
    FMOD_RESULT FMOD_System_GetMasterChannelGroup(FMOD_SYSTEM *, FMOD_CHANNELGROUP **);
    FMOD_RESULT FMOD_ChannelGroup_GetDSP(FMOD_CHANNELGROUP *, int, FMOD_DSP **);
    FMOD_RESULT FMOD_DSP_SetMeteringEnabled(FMOD_DSP *, FMOD_BOOL, FMOD_BOOL);
    FMOD_RESULT FMOD_DSP_GetMeteringInfo(FMOD_DSP *, FMOD_DSP_METERING_INFO *, FMOD_DSP_METERING_INFO *);
    FMOD_RESULT FMOD_System_MixerSuspend(FMOD_SYSTEM *);
    FMOD_RESULT FMOD_System_MixerResume(FMOD_SYSTEM *);
    FMOD_RESULT FMOD_System_GetVersion(FMOD_SYSTEM *, unsigned int *, unsigned int *);
    FMOD_RESULT FMOD_System_GetOutput(FMOD_SYSTEM *, FMOD_OUTPUTTYPE *);
    FMOD_RESULT FMOD_System_GetSoftwareFormat(FMOD_SYSTEM *, int *, FMOD_SPEAKERMODE *, int *);
    FMOD_RESULT FMOD_System_GetCPUUsage(FMOD_SYSTEM *, FMOD_CPU_USAGE *);
    FMOD_RESULT FMOD_Studio_System_Create(FMOD_STUDIO_SYSTEM **, unsigned int);
    FMOD_RESULT FMOD_Studio_System_GetCoreSystem(FMOD_STUDIO_SYSTEM *, FMOD_SYSTEM **);
    FMOD_RESULT FMOD_Studio_System_Initialize(FMOD_STUDIO_SYSTEM *, int, FMOD_STUDIO_INITFLAGS, FMOD_INITFLAGS, void *);
    FMOD_RESULT FMOD_Studio_System_Update(FMOD_STUDIO_SYSTEM *);
    FMOD_RESULT FMOD_Studio_System_Release(FMOD_STUDIO_SYSTEM *);
    FMOD_RESULT FMOD_Studio_System_LoadBankFile(FMOD_STUDIO_SYSTEM *, const char *, FMOD_STUDIO_LOAD_BANK_FLAGS,
                                                FMOD_STUDIO_BANK **);
    FMOD_RESULT FMOD_Studio_System_GetEvent(FMOD_STUDIO_SYSTEM *, const char *, FMOD_STUDIO_EVENTDESCRIPTION **);
    FMOD_RESULT FMOD_Studio_System_GetBankList(FMOD_STUDIO_SYSTEM *, FMOD_STUDIO_BANK **, int, int *);
    FMOD_RESULT FMOD_Studio_Bank_LoadSampleData(FMOD_STUDIO_BANK *);
    FMOD_RESULT FMOD_Studio_Bank_GetEventList(FMOD_STUDIO_BANK *, FMOD_STUDIO_EVENTDESCRIPTION **, int, int *);
    FMOD_RESULT FMOD_Studio_EventDescription_GetPath(FMOD_STUDIO_EVENTDESCRIPTION *, char *, int, int *);
    FMOD_RESULT FMOD_Studio_EventDescription_CreateInstance(FMOD_STUDIO_EVENTDESCRIPTION *,
                                                            FMOD_STUDIO_EVENTINSTANCE **);
    FMOD_RESULT FMOD_Studio_EventInstance_Start(FMOD_STUDIO_EVENTINSTANCE *);
    FMOD_RESULT FMOD_Studio_EventInstance_GetPlaybackState(FMOD_STUDIO_EVENTINSTANCE *, FMOD_STUDIO_PLAYBACK_STATE *);
#ifdef __cplusplus
}
#endif
