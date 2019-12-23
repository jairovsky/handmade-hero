#include "handmade.h"
#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <fileapi.h>
#include "win32_handmade.h"

// TODO fix this global
global_var bool running = true;
global_var win32_buffer backbuffer;
global_var LPDIRECTSOUNDBUFFER soundBuf;
global_var int64_t perfCounterFrequency;

#define KeyWasDown(param) ((param & (1 << 30))) != 0
#define KeyIsDown(param) ((param & (1 << 31))) == 0
#define AltIsDown(param) ((param & (1 << 29))) != 0

#define GAME_UPDATE_HZ 30
#define TARGET_SECS_PER_FRAME (1.0f / GAME_UPDATE_HZ)

win32_window_dimension
win32GetWindowDimension(HWND hwnd)
{
    win32_window_dimension dim;
    RECT rect;
    GetClientRect(hwnd, &rect);
    dim.width = rect.right - rect.left;
    dim.height = rect.bottom - rect.top;
    return dim;
}

internal void
win32LoadXInput(void)
{
    HMODULE xinputLib = LoadLibrary("xinput1_4.dll");
    if (!xinputLib)
    {
        xinputLib = LoadLibrary("xinput9_1_0.dll");
    }
    if (!xinputLib)
    {
        xinputLib = LoadLibrary("xinput1_3.dll");
    }
    if (xinputLib)
    {
        XInputGetState = (xinput_get_state *)GetProcAddress(xinputLib, "XInputGetState");
        XInputSetState = (xinput_set_state *)GetProcAddress(xinputLib, "XInputSetState");
    }
}

internal void
win32InitDSound(HWND hwnd, int32_t samplesPerSec, int32_t bufSize)
{
    HMODULE dsoundLib = LoadLibrary("dsound.dll");
    if (dsoundLib)
    {
        DirectSoundCreate = (dsound_create *)GetProcAddress(dsoundLib, "DirectSoundCreate");
        LPDIRECTSOUND dsound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &dsound, 0)))
        {
            WAVEFORMATEX format = {};
            format.wFormatTag = WAVE_FORMAT_PCM;
            format.nChannels = 2;
            format.nSamplesPerSec = samplesPerSec;
            format.wBitsPerSample = 16;
            format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8;
            format.nAvgBytesPerSec = samplesPerSec * format.nBlockAlign;
            if (SUCCEEDED(dsound->SetCooperativeLevel(hwnd, DSSCL_PRIORITY)))
            {
                LPDIRECTSOUNDBUFFER dsoundBuf;
                DSBUFFERDESC bufDesc = {};
                bufDesc.dwSize = sizeof(bufDesc);
                bufDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
                bufDesc.dwBufferBytes = 0;
                if (SUCCEEDED(dsound->CreateSoundBuffer(&bufDesc, &dsoundBuf, 0)))
                {
                    if (SUCCEEDED(dsoundBuf->SetFormat(&format)))
                    {
                        OutputDebugString("dsound buffer created successfully\n");
                    }
                }
                bufDesc = {};
                bufDesc.dwSize = sizeof(bufDesc);
                bufDesc.dwBufferBytes = bufSize;
                bufDesc.lpwfxFormat = &format;
                bufDesc.dwFlags = DSBCAPS_GLOBALFOCUS;
                if (SUCCEEDED(dsound->CreateSoundBuffer(&bufDesc, &soundBuf, 0)))
                {
                    OutputDebugString("secondary dsound buffer created successfully");
                }
            }
        }
    }
}

internal void
win32ResizeDIBSection(win32_buffer *buf, int width, int height)
{
    if (buf->memory)
    {
        VirtualFree(buf->memory, 0, MEM_RELEASE);
    }
    buf->width = width;
    buf->height = height;
    buf->info.bmiHeader.biSize = sizeof(buf->info.bmiHeader);
    buf->info.bmiHeader.biWidth = buf->width;
    buf->info.bmiHeader.biHeight = -buf->height;
    buf->info.bmiHeader.biPlanes = 1;
    buf->info.bmiHeader.biBitCount = 32;
    buf->info.bmiHeader.biCompression = BI_RGB;
    //after clarification about stretchDIBits and BitBlt, we know
    // we can allocate the memory ourselves
    int bitmapMemorySize = buf->bytesPerPixel * buf->width * buf->height;
    buf->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    buf->pitch = buf->width * buf->bytesPerPixel;
    // TODO maybe clear to black
}

internal void
win32DisplayBufferInWindow(win32_buffer *buf, HDC hdc, int wWidth, int wHeight)
{
    StretchDIBits(
        hdc,
        0, 0, wWidth, wHeight,
        0, 0, buf->width, buf->height,
        buf->memory,
        &buf->info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

LRESULT CALLBACK
MainWndCallback(HWND hwnd,
                UINT uMsg,
                WPARAM wParam,
                LPARAM lParam)
{
    LRESULT r = 0;

    switch (uMsg)
    {
    case WM_SIZE:
    {
        break;
    }

    case WM_DESTROY:
    {
        running = false;
        OutputDebugString("WM_DESTROY called\n");
        break;
    }

    case WM_CLOSE:
    {
        running = false;
        OutputDebugString("WM_CLOSE event received\n");
        break;
    }

    case WM_PAINT:
    {
        win32_window_dimension d = win32GetWindowDimension(hwnd);
        PAINTSTRUCT p;
        HDC devCtx = BeginPaint(hwnd, &p);
        win32DisplayBufferInWindow(&backbuffer, devCtx, d.width, d.height);
        EndPaint(hwnd, &p);
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        assert(!"keyboard input event came from non-dispatch message");
        break;
    }

    default:
        r = DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return r;
}

internal void
win32ClearSoundBuffer(win32_sound_output *soundOutput)
{
    VOID *region1;
    DWORD region1Size;
    VOID *region2;
    DWORD region2Size;
    if (SUCCEEDED(soundBuf->Lock(
            0, soundOutput->soundBufSize,
            &region1, &region1Size,
            &region2, &region2Size,
            0)))
    {
        DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
        uint8_t *dest = (uint8_t *)region1;
        for (DWORD i = 0; i < region1SampleCount; ++i)
        {
            *dest++ = 0;
        }
        DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
        dest = (uint8_t *)region2;
        for (DWORD i = 0; i < region2SampleCount; ++i)
        {
            *dest++ = 0;
        }
        soundBuf->Unlock(region1, region1Size, region2, region2Size);
    }
}

internal void
win32FillSoundBuffer(
    win32_sound_output *soundOutput,
    DWORD byteToLock,
    DWORD bytesToWrite,
    game_sound_buffer *gsBuf)
{
    VOID *region1;
    DWORD region1Size;
    VOID *region2;
    DWORD region2Size;
    if (SUCCEEDED(soundBuf->Lock(
            byteToLock, bytesToWrite,
            &region1, &region1Size,
            &region2, &region2Size,
            0)))
    {
        DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
        int16_t *dest = (int16_t *)region1;
        int16_t *source = gsBuf->samples;
        for (DWORD i = 0; i < region1SampleCount; ++i)
        {
            *dest++ = *source++;
            *dest++ = *source++;
            ++soundOutput->runningSampleIdx;
        }
        DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
        dest = (int16_t *)region2;
        for (DWORD i = 0; i < region2SampleCount; ++i)
        {
            *dest++ = *source++;
            *dest++ = *source++;
            ++soundOutput->runningSampleIdx;
        }
        soundBuf->Unlock(region1, region1Size, region2, region2Size);
    }
}

internal void
win32ProcessXInputBtn(WORD btnStateBits,
                      WORD btnBit,
                      game_button_state *oldState,
                      game_button_state *newState)
{
    newState->endedDown = (btnStateBits & btnBit) == btnBit;
    newState->nHalfTransitions = (oldState->endedDown != newState->endedDown) ? 1 : 0;
}

void win32ProcessKeyboardInput(game_button_state *newState, bool isDown)
{
    /* TODO(jairo): investigate if this assertion is valid/necessary.
    Right now it triggers when I use ctrl+win+arrows to navigate
    between Windows 10 workspaces.
    assert(newState->endedDown != isDown); */
    newState->endedDown = isDown;
    newState->nHalfTransitions++;
}

internal void
win32NormalizeXInputThumbstick(SHORT val, SHORT deadzone, float *normalizedVal)
{
    if (val < -deadzone)
    {
        *normalizedVal = (float)(val + deadzone) / (SHRT_MIN * -1.0f - deadzone);
    }
    else if (val > deadzone)
    {
        *normalizedVal = (float)(val - deadzone) / (SHRT_MAX - deadzone);
    }
}

internal void win32ProcessPendingMessages(game_controller_input *keyboardController)
{
    MSG msg;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            OutputDebugString("quitting");
            running = false;
        }
        switch (msg.message)
        {
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32_t kCode = (uint32_t)msg.wParam;
            bool isDown = KeyIsDown(msg.lParam);
            bool wasDown = KeyWasDown(msg.lParam);
            /* NOTE(jairo): while doing some tests with the keyboard
            input, I found a bug. If I hold any key for about 3-4 seconds,
            Windows sends me a WM_KEYDOWN event with the wasDown flag set to false!
            (it's as if I had stopped pressing the key). To fix this, we need to
            check the "repeat count" bits of lParam to see if the event was caused
            by an actual keypress or by Windows auto-key-repeat feature.
            */
            bool isNotKeyRepeat = (HIWORD(msg.lParam) & KF_REPEAT) == 0;
            // NOTE(jairo): it's impossible to receive a key-repeat event if it's
            // keyup, so we just ignore
            if (msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP)
            {
                isNotKeyRepeat = true;
            }
            if (isDown != wasDown && isNotKeyRepeat)
            {
                switch (kCode)
                {
                case 'W':
                    win32ProcessKeyboardInput(&keyboardController->moveUp, isDown);
                    break;
                case 'S':
                    win32ProcessKeyboardInput(&keyboardController->moveDown, isDown);
                    break;
                case 'D':
                    win32ProcessKeyboardInput(&keyboardController->moveRight, isDown);
                    break;
                case 'A':
                    win32ProcessKeyboardInput(&keyboardController->moveLeft, isDown);
                    break;
                case 'Q':
                    win32ProcessKeyboardInput(&keyboardController->leftBumper, isDown);
                    break;
                case 'E':
                    win32ProcessKeyboardInput(&keyboardController->rightBumper, isDown);
                    break;
                case VK_LEFT:
                    win32ProcessKeyboardInput(&keyboardController->action0, isDown);
                    break;
                case VK_DOWN:
                    win32ProcessKeyboardInput(&keyboardController->action1, isDown);
                    break;
                case VK_RIGHT:
                    win32ProcessKeyboardInput(&keyboardController->action2, isDown);
                    break;
                case VK_UP:
                    win32ProcessKeyboardInput(&keyboardController->action3, isDown);
                    break;
                case VK_ESCAPE:
                    running = false;
                    break;
                case VK_SPACE:
                    break;
                case VK_F4:
                    if (AltIsDown(msg.lParam))
                    {
                        running = false;
                    }
                }
            }
            break;
        }
        default:
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

debug_read_file_result DEBUGplatformReadFile(char *filename)
{
    debug_read_file_result result = {};
    HANDLE hFile = CreateFile(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(hFile, &fileSize))
        {
            uint32_t fileSize32 = safeTruncateUint64(fileSize.QuadPart);
            result.content = VirtualAlloc(0, (uint32_t)fileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            DWORD nRead;
            if (ReadFile(hFile, result.content, fileSize32, &nRead, 0) && nRead == fileSize32)
            {
                result.size = nRead;
            }
            else
            {
                DEBUGplatformFreeFile(result.content);
                result.content = 0;
                result.size = 0;
            }
        }
        CloseHandle(hFile);
    }
    else
    {
        DWORD err = GetLastError();
        DEBUG("DBG error while opening file: error code %d\n", err);
    }
    return result;
}

void DEBUGplatformFreeFile(void *file)
{
    VirtualFree(file, 0, MEM_RELEASE);
}

bool DEBUGplatformWriteFile(char *filename, uint32_t size, void *content)
{
    bool result = false;
    HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD nWritten;
        if (WriteFile(hFile, content, size, &nWritten, 0) && nWritten == size)
        {
            result = true;
        }
    }
    return result;
}

inline LARGE_INTEGER
win32GetWallclock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

inline float
win32GetSecondsDiff(LARGE_INTEGER end, LARGE_INTEGER start)
{
    return (float)(end.QuadPart - start.QuadPart) / (float)perfCounterFrequency;
}

internal void
win32DebugDrawVertical(win32_buffer *backBuf, int x, int top, int bottom, uint32_t color)
{
    uint8_t *pixel = (uint8_t *)backBuf->memory + x * backBuf->bytesPerPixel + top * backBuf->pitch;
    for(int y = top; y < bottom; y++)
    {
        *(uint32_t*)pixel = color;
        pixel += backBuf->pitch;
    }
}

internal void
win32DebugSyncDisplay(win32_buffer *backBuf, int markerCount, win32_debug_time_marker *markers,
                      win32_sound_output *sound, float secPerFrame, int currentMarker)
{
    int padx = 16;
    int pady = 64;
    int top = pady;
    int bottom = backbuffer.height - pady*5;
    float coeff = ((float)backBuf->width - (2 * padx)) / (float)sound->soundBufSize;
    for (int i = 0; i < markerCount; i++)
    {
        win32_debug_time_marker *currMarker = &markers[i];
        assert(currMarker->playCursor < sound->soundBufSize);
        float playFloat = (float)currMarker->playCursor;
        float writeFloat = (float)currMarker->writeCursor;
        int playX = padx + (int)(coeff * playFloat);
        int writeX = padx + (int)(coeff * writeFloat);
        if (i == currentMarker)
        {
        win32DebugDrawVertical(backBuf, playX, top, bottom, 0x00ff0000);
        win32DebugDrawVertical(backBuf, writeX, top*5, bottom + pady*2, 0x00ff0000);
        }
        else
        {
        win32DebugDrawVertical(backBuf, playX, top, bottom, 0xffffffff);
        win32DebugDrawVertical(backBuf, writeX, top*5, bottom + pady*2, 0xffffffff);
        }
    }

}

internal void
win32DebugAudioCursorDistance(LPDIRECTSOUNDBUFFER sBuf, win32_sound_output soundOutput)
{
    DWORD playCursor;
    DWORD writeCursor;
    sBuf->GetCurrentPosition(&playCursor, &writeCursor);
    if (writeCursor < playCursor) // if cursor is wrapping around the circular buffer
    {
        writeCursor += soundOutput.soundBufSize;
    }
    DWORD bytesBetweenCursors = writeCursor - playCursor;
    DEBUG("DBG bytesBetweenCursors: %u\n", bytesBetweenCursors);
    DWORD nSamplesBetweenCursors = bytesBetweenCursors / soundOutput.bytesPerSample;
    float audioLatencyInSeconds = (float)nSamplesBetweenCursors / (float)soundOutput.samplePerSec;
    DEBUG("DBG audioLatencyInSeconds: %.5f\n", audioLatencyInSeconds);
}

int CALLBACK
WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nShowCmd)
{
    LARGE_INTEGER perfFreq;
    QueryPerformanceFrequency(&perfFreq);
    perfCounterFrequency = perfFreq.QuadPart;
    UINT schedulerGranularityMs = 1;
    bool sleepIsGranular = (timeBeginPeriod(schedulerGranularityMs) == TIMERR_NOERROR);

    win32LoadXInput();
    win32ResizeDIBSection(&backbuffer, 1280, 720);
    WNDCLASS wc = {};
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndCallback;
    wc.hInstance = hInstance;
    wc.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&wc))
    {
        HWND wnd = CreateWindowEx(
            0,
            wc.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            hInstance,
            0);

        if (wnd)
        {
            HDC hdc = GetDC(wnd);
            win32_sound_output soundOutput = {};
            soundOutput.samplePerSec = 48000;
            soundOutput.bytesPerSample = sizeof(int16_t) * 2;
            soundOutput.soundBufSize = soundOutput.samplePerSec * soundOutput.bytesPerSample;
            soundOutput.nLatencySamples = 3 * (soundOutput.samplePerSec / GAME_UPDATE_HZ);
            soundOutput.safetyBytes = (soundOutput.samplePerSec * soundOutput.bytesPerSample / GAME_UPDATE_HZ) / 3;
            win32InitDSound(wnd, soundOutput.samplePerSec, soundOutput.soundBufSize);
            win32ClearSoundBuffer(&soundOutput);
            soundBuf->Play(0, 0, DSBPLAY_LOOPING);
            int16_t *soundSamples = (int16_t *)VirtualAlloc(0, soundOutput.soundBufSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#if HANDMADE_INTERNAL
            LPVOID baseAddress = (LPVOID)(2 * TERABYTE);
#else
            LPVOID baseAddress = 0;
#endif
            game_memory gameMemory = {};
            gameMemory.permStorageSize = 64 * MEGABYTE;
            gameMemory.transientStorageSize = 1 * GIGABYTE;
            uint64_t totalStorageSize = gameMemory.permStorageSize + gameMemory.transientStorageSize;
            gameMemory.permStorage = VirtualAlloc(baseAddress, (uint32_t)totalStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            gameMemory.transientStorage = ((uint8_t *)gameMemory.permStorage + gameMemory.permStorageSize);
            game_input input[2] = {};
            game_input *newInput = &input[0];
            game_input *oldInput = &input[1];
#if HANDMADE_INTERNAL
            win32_debug_time_marker debugTimeMarkers[GAME_UPDATE_HZ / 2] = {0};
            DWORD debugLastTimeMarkerIndex = 0;
#endif
            DWORD lastPlayCursor = 0;
            DWORD lastWriteCursor = 0;
            DWORD soundIsValid = false;
            LARGE_INTEGER perfCounterStart = win32GetWallclock();
            while (running)
            {
                game_controller_input *oldKeyboardController = getController(oldInput, 0);
                game_controller_input *newKeyboardController = getController(newInput, 0);
                *newKeyboardController = {};
                newKeyboardController->isConnected = true;
                for (uint8_t btnIdx = 0; btnIdx < arrayCount(newKeyboardController->buttons); btnIdx++)
                {
                    newKeyboardController->buttons[btnIdx].endedDown =
                        oldKeyboardController->buttons[btnIdx].endedDown;
                }
                win32ProcessPendingMessages(newKeyboardController);
                for (WORD ctrlIdx = 0; ctrlIdx < XUSER_MAX_COUNT; ++ctrlIdx)
                {
                    WORD internalCtrlIdx = ctrlIdx + 1;
                    game_controller_input *oldController = getController(oldInput, internalCtrlIdx);
                    game_controller_input *newController = getController(newInput, internalCtrlIdx);
                    XINPUT_STATE inputState;
                    DWORD gotInput = XInputGetState(ctrlIdx, &inputState);
                    if (gotInput == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD *pad = &inputState.Gamepad;
                        newController->stickAverageX = 0.0f;
                        newController->stickAverageY = 0.0f;
                        win32NormalizeXInputThumbstick(pad->sThumbLX,
                                                       XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE,
                                                       &newController->stickAverageX);
                        win32NormalizeXInputThumbstick(pad->sThumbLY,
                                                       XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE,
                                                       &newController->stickAverageY);

                        if (newController->stickAverageX != 0.0f || newController->stickAverageY != 0.0f)
                        {
                            newController->isAnalog = true;
                        }
                        newController->isConnected = true;
                        bool padUp = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool padDown = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool padLeft = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool padRight = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        if (padUp)
                        {
                            newController->stickAverageY = 1;
                            newController->isAnalog = false;
                        }
                        if (padDown)
                        {
                            newController->stickAverageY = -1;
                            newController->isAnalog = false;
                        }
                        if (padLeft)
                        {
                            newController->stickAverageX = -1;
                            newController->isAnalog = false;
                        }
                        if (padRight)
                        {
                            newController->stickAverageX = 1;
                            newController->isAnalog = false;
                        }
                        // NOTE(jairo): converting analog stick input to digital arrows
                        float stickThreshold = 0.5f;
                        win32ProcessXInputBtn((newController->stickAverageX < -stickThreshold) ? 1 : 0,
                                              1, &oldController->moveLeft, &newController->moveLeft);
                        win32ProcessXInputBtn((newController->stickAverageX > stickThreshold) ? 1 : 0,
                                              1, &oldController->moveRight, &newController->moveRight);
                        win32ProcessXInputBtn((newController->stickAverageY < -stickThreshold) ? 1 : 0,
                                              1, &oldController->moveDown, &newController->moveDown);
                        win32ProcessXInputBtn((newController->stickAverageY > stickThreshold) ? 1 : 0,
                                              1, &oldController->moveUp, &newController->moveUp);
                        // end conversion
                        win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_A,
                                              &oldController->action1, &newController->action1);
                        win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_B,
                                              &oldController->action2, &newController->action2);
                        win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_X,
                                              &oldController->action0, &newController->action0);
                        win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_Y,
                                              &oldController->action3, &newController->action3);
                        win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER,
                                              &oldController->leftBumper, &newController->leftBumper);
                        win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                              &oldController->rightBumper, &newController->rightBumper);
                        win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_START,
                                              &oldController->start, &newController->start);
                        win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_BACK,
                                              &oldController->select, &newController->select);
                    }
                    else
                    {
                        // TODO handle controller disconnected
                    }
                }

                game_offscreen_buffer buf = {};
                buf.memory = backbuffer.memory;
                buf.width = backbuffer.width;
                buf.height = backbuffer.height;
                buf.pitch = backbuffer.pitch;

                gameUpdateAndRender(&gameMemory, newInput, &buf);

                DWORD playCursor;
                DWORD writeCursor;
                if (soundBuf->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
                {
                    if (!soundIsValid)
                    {
                        soundOutput.runningSampleIdx = writeCursor / soundOutput.bytesPerSample;
                        soundIsValid = true;
                    }
                    DWORD byteToLock = (soundOutput.runningSampleIdx * soundOutput.bytesPerSample) % soundOutput.soundBufSize;
                    DWORD expectedSoundBytesPerFrame = (soundOutput.samplePerSec * soundOutput.bytesPerSample) / GAME_UPDATE_HZ;
                    DWORD expectedFrameBoundaryByte = playCursor + expectedSoundBytesPerFrame;
                    DWORD safeWriteCursor = writeCursor;
                    if (safeWriteCursor < playCursor)
                    {
                        safeWriteCursor += soundOutput.soundBufSize;
                    }
                    assert(safeWriteCursor >= playCursor);
                    safeWriteCursor += soundOutput.safetyBytes;
                    bool audioCardIsLowLatency = safeWriteCursor < expectedFrameBoundaryByte;
                    DWORD targetCursor = 0;
                    if(audioCardIsLowLatency)
                    {
                        targetCursor = (expectedFrameBoundaryByte + expectedSoundBytesPerFrame);
                    }
                    else
                    {
                        targetCursor = (writeCursor + expectedSoundBytesPerFrame + soundOutput.safetyBytes);
                    }
                    targetCursor = targetCursor % soundOutput.soundBufSize;

                    DWORD bytesToWrite = 0;
                    if (byteToLock > targetCursor)
                    {
                        bytesToWrite = (soundOutput.soundBufSize - byteToLock) + targetCursor;
                    }
                    else
                    {
                        bytesToWrite = targetCursor - byteToLock;
                    }

                    game_sound_buffer sBuf = {};
                    sBuf.samplesPerSec = soundOutput.samplePerSec;
                    sBuf.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                    sBuf.samples = soundSamples;
                    gameGetSoundSamples(&gameMemory, &sBuf);
#if HANDMADE_INTERNAL
                    win32DebugAudioCursorDistance(soundBuf, soundOutput);
#endif
                    win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &sBuf);
                }

                LARGE_INTEGER workFinishedCounter = win32GetWallclock();
                float timeSpentOnActualWork = win32GetSecondsDiff(workFinishedCounter, perfCounterStart);
                float timeToFlipFrame = timeSpentOnActualWork;
                if (timeToFlipFrame < TARGET_SECS_PER_FRAME)
                {
                    while (timeToFlipFrame < TARGET_SECS_PER_FRAME)
                    {
                        if (sleepIsGranular)
                        {
                            DWORD sleepMs = (DWORD)(1000.0f * (TARGET_SECS_PER_FRAME - timeToFlipFrame));
                            if (sleepMs > 0)
                            {
                                Sleep(sleepMs);
                            }
                        }
                        timeToFlipFrame = win32GetSecondsDiff(win32GetWallclock(), perfCounterStart);
                    }
                }
                else
                {
                    // TODO: handle missed frame
                    DEBUG("DBG missed a frame\n");
                }
                perfCounterStart = win32GetWallclock();

#if HANDMADE_INTERNAL
                win32DebugSyncDisplay(
                    &backbuffer,
                    arrayCount(debugTimeMarkers),
                    debugTimeMarkers,
                    &soundOutput,
                    TARGET_SECS_PER_FRAME,
                    debugLastTimeMarkerIndex -1);
#endif
                win32_window_dimension d = win32GetWindowDimension(wnd);
                win32DisplayBufferInWindow(&backbuffer, hdc, d.width, d.height);
                if(soundBuf->GetCurrentPosition(&lastPlayCursor, &lastWriteCursor) == DS_OK)
                {
                    soundIsValid = true;
                }
                else
                {
                    soundIsValid = false;
                }
#if HANDMADE_INTERNAL
                win32_debug_time_marker *debugMarker = &debugTimeMarkers[debugLastTimeMarkerIndex++];
                if (debugLastTimeMarkerIndex == arrayCount(debugTimeMarkers))
                {
                    debugLastTimeMarkerIndex = 0;
                }
                debugMarker->playCursor = lastPlayCursor;
                debugMarker->writeCursor = lastWriteCursor;
#endif

                game_input *temp = newInput;
                newInput = oldInput;
                oldInput = temp;

            }
        }
        else
        {
            //TODO
        }
    }
    else
    {
        //TODO()
    }
    return 0;
}
