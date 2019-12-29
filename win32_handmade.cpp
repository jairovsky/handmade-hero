#include "handmade.h"
#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <fileapi.h>
#include "win32_handmade.h"

// TODO fix this global
global_var bool running = true;
global_var win32_offscreen_buffer backbuffer;
global_var LPDIRECTSOUNDBUFFER soundBuf;
global_var int64_t perfCounterFrequency;

#define KeyWasDown(param) ((param & (1 << 30))) != 0
#define KeyIsDown(param) ((param & (1 << 31))) == 0
#define AltIsDown(param) ((param & (1 << 29))) != 0

#define SCHEDULER_GRANULARITY_MS 1

inline uint32_t
safeTruncateUint64(uint64_t val)
{
    assert(val <= UINT_MAX);
    return (uint32_t)val;
}

struct win32_game_code
{
    HMODULE gameCodeDll;
    FILETIME lastUpdated;
    game_update_and_render *updateAndRender;
    game_get_sound_samples *getSoundSamples;
};

internal void
win32UnloadGameCode(win32_game_code *gameCode)
{
    if (gameCode->gameCodeDll)
    {
        FreeLibrary(gameCode->gameCodeDll);
        gameCode->updateAndRender = 0;
        gameCode->getSoundSamples = 0;
    }
}

internal void
win32LoadGameCode(win32_game_code *gameCode)
{
    //TODO(jairo): handle executable path handling as Casey did

#define dllFileName "handmade.dll"
#define dllTmpFileName "tmp_handmade.dll"

    HANDLE hFile = CreateFile(dllFileName, GENERIC_READ, FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    FILETIME creationTime;
    FILETIME lastAccessTime;
    FILETIME lastWriteTime;
    if (GetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime))
    {
        CloseHandle(hFile);

        bool needsReloading = CompareFileTime(&gameCode->lastUpdated, &lastWriteTime) < 0;
        if (needsReloading)
        {
            DEBUG("DBG reloading dll %s", dllFileName);
            win32UnloadGameCode(gameCode);

            CopyFile(dllFileName, dllTmpFileName, false);
            HMODULE gameCodeDll = LoadLibrary(dllTmpFileName);
            if (gameCodeDll)
            {
                gameCode->gameCodeDll = gameCodeDll;
                gameCode->updateAndRender = (game_update_and_render *)GetProcAddress(gameCodeDll, "gameUpdateAndRender");
                gameCode->getSoundSamples = (game_get_sound_samples *)GetProcAddress(gameCodeDll, "gameGetSoundSamples");

                gameCode->lastUpdated = lastWriteTime;
            }
        }
    }
    else
    {
        DEBUG("DBG couldn't open dll file %s\n", dllFileName);
    }
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
win32ResizeDIBSection(win32_offscreen_buffer *buf, int width, int height)
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
win32DisplayBufferInWindow(win32_offscreen_buffer *buf, HDC hdc, int wWidth, int wHeight)
{
    StretchDIBits(
        hdc,
        0, 0, buf->width, buf->height,
        0, 0, buf->width, buf->height,
        buf->memory,
        &buf->info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

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

DEBUG_PLATFORM_FREE_FILE(DEBUGplatformFreeFile)
{
    VirtualFree(file, 0, MEM_RELEASE);
}

DEBUG_PLATFORM_READ_FILE(DEBUGplatformReadFile)
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

DEBUG_PLATFORM_WRITE_FILE(DEBUGplatformWriteFile)
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
win32DebugDrawVerticalLine(win32_offscreen_buffer *backBuf, int x, int top, int bottom, uint32_t color)
{
    uint8_t *pixel = (uint8_t *)backBuf->memory + x * backBuf->bytesPerPixel + top * backBuf->pitch;
    for(int y = top; y < bottom; y++)
    {
        *(uint32_t*)pixel = color;
        pixel += backBuf->pitch;
    }
}

internal void
win32DebugDrawAudioCursors(win32_offscreen_buffer *backBuf,
                           int markerCount,
                           win32_debug_time_marker *markers,
                           win32_sound_output *sound,
                           float secPerFrame,
                           int currentMarkerIdx)
{

#define red 0x00ff0000
#define white 0x00ffffff

    int paddingLeft = 16;
    int paddingTop = 64;

    int lineStart = paddingTop;
    int lineEnd = lineStart + 64;

    float screenWidthToSoundBufferSizeRatio = ((float)backBuf->width - (2 * paddingLeft)) / (float)sound->soundBufSize;

    for (int i = 0; i < markerCount; i++)
    {
        win32_debug_time_marker *marker = &markers[i];
        assert(marker->playCursor < sound->soundBufSize);

        int playX = paddingLeft + (int)(screenWidthToSoundBufferSizeRatio * marker->playCursor);
        int writeX = paddingLeft + (int)(screenWidthToSoundBufferSizeRatio * marker->writeCursor);

        if (i == currentMarkerIdx)
        {
            win32DebugDrawVerticalLine(backBuf, playX, lineStart, lineEnd, red);
            win32DebugDrawVerticalLine(backBuf, writeX, lineStart + 70, lineStart + 128, red);
        }
        else
        {
            win32DebugDrawVerticalLine(backBuf, playX, lineStart, lineEnd, white);
            win32DebugDrawVerticalLine(backBuf, writeX, lineStart + 70, lineStart + 128, white);
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

            int gameUpdateHz = GetDeviceCaps(hdc, VREFRESH);
            float targetSecsPerFrame = 1.0f / gameUpdateHz;

            win32ResizeDIBSection(&backbuffer, 960, 540);

            win32_game_code game = {};
            win32LoadGameCode(&game);

            win32_sound_output soundOutput = {};
            soundOutput.samplePerSec = 48000;
            soundOutput.bytesPerSample = sizeof(int16_t) * 2;
            soundOutput.soundBufSize = soundOutput.samplePerSec * soundOutput.bytesPerSample;
            soundOutput.safetyBytes = (soundOutput.samplePerSec * soundOutput.bytesPerSample / gameUpdateHz);

            int16_t *soundMemory = (int16_t *)VirtualAlloc(0, soundOutput.soundBufSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

            win32InitDSound(wnd, soundOutput.samplePerSec, soundOutput.soundBufSize);
            win32ClearSoundBuffer(&soundOutput);
            soundBuf->Play(0, 0, DSBPLAY_LOOPING);

            DWORD lastSoundPlayCursor = 0;
            DWORD lastSoundWriteCursor = 0;
            bool soundIsValid = false;

            game_memory gameMemory = {};
            gameMemory.permStorageSize = 64 * MEGABYTE;
            gameMemory.transientStorageSize = 1 * GIGABYTE;
            uint64_t totalStorageSize = gameMemory.permStorageSize + gameMemory.transientStorageSize;
            gameMemory.permStorage = VirtualAlloc(HANDMADE_BASE_GAME_MEM_ADDR, (uint32_t)totalStorageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            gameMemory.transientStorage = ((uint8_t *)gameMemory.permStorage + gameMemory.permStorageSize);
            gameMemory.DEBUGplatformReadFile = &DEBUGplatformReadFile;
            gameMemory.DEBUGplatformWriteFile = &DEBUGplatformWriteFile;
            gameMemory.DEBUGplatformFreeFile = &DEBUGplatformFreeFile;

            win32LoadXInput();

            game_input input[2] = {};
            game_input *newInput = &input[0];
            game_input *oldInput = &input[1];

            LARGE_INTEGER perfFreq;
            QueryPerformanceFrequency(&perfFreq);
            perfCounterFrequency = perfFreq.QuadPart;
            bool sleepIsGranular = (timeBeginPeriod(SCHEDULER_GRANULARITY_MS) == TIMERR_NOERROR);

            LARGE_INTEGER perfCounterStart = win32GetWallclock();

#if HANDMADE_INTERNAL
            win32_debug_time_marker debugTimeMarkers[15] = {0};
            DWORD debugLastTimeMarkerIndex = 0;
#endif

            while (running)
            {

#if HANDMADE_INTERNAL
                win32LoadGameCode(&game);
#endif

                game_input *temp = newInput;
                newInput = oldInput;
                oldInput = temp;
                newInput->secsToComputeUpdate = targetSecsPerFrame;

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
                    WORD gamepadIdx = ctrlIdx + 1;
                    game_controller_input *oldController = getController(oldInput, gamepadIdx);
                    game_controller_input *newController = getController(newInput, gamepadIdx);

                    XINPUT_STATE gamepadInput;
                    DWORD gotInput = XInputGetState(ctrlIdx, &gamepadInput);
                    if (gotInput == ERROR_SUCCESS)
                    {
                        newController->isConnected = true;

                        XINPUT_GAMEPAD *pad = &gamepadInput.Gamepad;

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
                buf.bytesPerPixel = backbuffer.bytesPerPixel;

                game.updateAndRender(&gameMemory, newInput, &buf);

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
                    DWORD expectedSoundBytesPerFrame = (soundOutput.samplePerSec * soundOutput.bytesPerSample) / gameUpdateHz;
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
                    sBuf.samples = soundMemory;

                    game.getSoundSamples(&gameMemory, &sBuf);
#if UNDEF
                    win32DebugAudioCursorDistance(soundBuf, soundOutput);
#endif
                    win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &sBuf);
                }

                LARGE_INTEGER workFinishedCounter = win32GetWallclock();
                float timeSpentOnActualWork = win32GetSecondsDiff(workFinishedCounter, perfCounterStart);
                float timeToFlipFrame = timeSpentOnActualWork;
                if (timeToFlipFrame < targetSecsPerFrame)
                {
                    while (timeToFlipFrame < targetSecsPerFrame)
                    {
                        if (sleepIsGranular)
                        {
                            DWORD sleepMs = (DWORD)(1000.0f * (targetSecsPerFrame - timeToFlipFrame));
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

#ifdef UNDEF
                win32DebugDrawAudioCursors(
                    &backbuffer,
                    arrayCount(debugTimeMarkers),
                    debugTimeMarkers,
                    &soundOutput,
                    targetSecsPerFrame,
                    debugLastTimeMarkerIndex -1);
#endif

                win32_window_dimension d = win32GetWindowDimension(wnd);
                win32DisplayBufferInWindow(&backbuffer, hdc, d.width, d.height);

#ifdef UNDEF
                win32_debug_time_marker *debugMarker = &debugTimeMarkers[debugLastTimeMarkerIndex++];
                debugMarker->playCursor = lastSoundPlayCursor;
                debugMarker->writeCursor = lastSoundWriteCursor;
                if (debugLastTimeMarkerIndex == arrayCount(debugTimeMarkers))
                {
                    debugLastTimeMarkerIndex = 0;
                }
#endif
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
