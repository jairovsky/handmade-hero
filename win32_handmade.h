#ifndef WIN32_HANDMADE_H

#define DEBUG(...) {char cad[512]; sprintf_s(cad, __VA_ARGS__);  OutputDebugString(cad);}

struct win32_window_dimension
{
    int width;
    int height;
};

struct win32_buffer {
    BITMAPINFO info;
    void*      memory;
    int        width;
    int        height;
    int        pitch;
    int        bytesPerPixel = 4;
};

struct win32_sound_output
{
    DWORD samplePerSec;
    DWORD bytesPerSample;
    DWORD soundBufSize;
    uint32_t runningSampleIdx;
    uint32_t nLatencySamples;
};


#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(xinput_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_var xinput_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_




#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(xinput_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_var xinput_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_



#define DIRECTSOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECTSOUND_CREATE(dsound_create);
DIRECTSOUND_CREATE(DirectSoundCreateStub)
{
    return DSERR_NODRIVER;
}
global_var dsound_create* DirectSoundCreate_ = DirectSoundCreateStub;
#define DirectSoundCreate DirectSoundCreate_


struct win32_debug_time_marker
{
    DWORD playCursor;
    DWORD writeCursor;
};


#define WIN32_HANDMADE_H
#endif
