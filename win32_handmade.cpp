#include <stdint.h>
#include <math.h>
#include <cstdio>
#include <limits.h>
#include "handmade.cpp"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

// TODO fix this global
global_var bool running = true;
global_var win32_buffer backbuffer;
global_var LPDIRECTSOUNDBUFFER soundBuf;


#define KeyWasDown(param) ((param & (1 << 30))) != 0
#define KeyIsDown(param) ((param & (1 << 31))) == 0
#define AltIsDown(param) ((param & (1 << 29))) != 0

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
            DirectSoundCreate = (dsound_create*)GetProcAddress(dsoundLib, "DirectSoundCreate");
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
    if (buf->memory) {
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
                  SRCCOPY
                  );
}

LRESULT CALLBACK
MainWndCallback(HWND hwnd,
                UINT uMsg,
                WPARAM wParam,
                LPARAM lParam)
{
    LRESULT r = 0;

    switch (uMsg) {
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
            //PostQuitMessage(0);
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
            uint32_t kCode = wParam;
            if (KeyIsDown(lParam) != KeyWasDown(lParam))
                {
                    switch (kCode)
                        {
                        case 'W':
                            OutputDebugString("hey you pressed W on keyboard\n");
                            break;
                        case 'S':
                        case 'D':
                        case 'A':
                        case 'Q':
                        case 'E':
                        case VK_UP:
                        case VK_LEFT:
                        case VK_RIGHT:
                        case VK_DOWN:
                        case VK_ESCAPE:
                            running = false;
                            break;
                        case VK_SPACE:
                            break;

                        case VK_F4:
                            if (AltIsDown(lParam))
                                {
                                    running = false;
                                }
                        }
                }
            break;
        }

    default:
        {
            r = DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }

    return r;
}

internal void
win32ClearSoundBuffer(win32_sound_output *soundOutput)
{
    VOID* region1;
    DWORD region1Size;
    VOID* region2;
    DWORD region2Size;
    if (SUCCEEDED(soundBuf->Lock(
                                 0, soundOutput->soundBufSize,
                                 &region1, &region1Size,
                                 &region2, &region2Size,
                                 0)))
        {
            DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
            uint8_t* dest = (uint8_t*)region1;
            for (DWORD i = 0; i < region1SampleCount; ++i) {
                *dest++ = 0;
            }
            DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
            dest = (uint8_t*)region2;
            for (DWORD i = 0; i < region2SampleCount; ++i) {
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
    VOID* region1;
    DWORD region1Size;
    VOID* region2;
    DWORD region2Size;
    if (SUCCEEDED(soundBuf->Lock(
                                 byteToLock, bytesToWrite,
                                 &region1, &region1Size,
                                 &region2, &region2Size,
                                 0)))
        {
            DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
            int16_t* dest = (int16_t*)region1;
            int16_t* source = gsBuf->samples;
            for (DWORD i = 0; i < region1SampleCount; ++i)
                {
                    *dest++ = *source++;
                    *dest++ = *source++;
                    ++soundOutput->runningSampleIdx;
                }
            DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
            dest = (int16_t*)region2;
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
    newState->nHalfTransitions = (oldState->endedDown != newState->endedDown);
}

int CALLBACK
WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        int       nShowCmd)
{
    LARGE_INTEGER perfFreq;
    QueryPerformanceFrequency(&perfFreq);

    win32LoadXInput();
    win32ResizeDIBSection(&backbuffer, 1280, 720);
    WNDCLASS wc = {};
    wc.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc = MainWndCallback;
    wc.hInstance = hInstance;
    wc.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&wc)) {
        HWND wnd = CreateWindowEx(
                                  0,
                                  wc.lpszClassName,
                                  "Handmade Hero",
                                  WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  0,
                                  0,
                                  hInstance,
                                  0);

        if (wnd) {
            HDC hdc = GetDC(wnd);
            MSG msg;
            win32_sound_output soundOutput = {};
            soundOutput.samplePerSec = 48000;
            soundOutput.bytesPerSample = sizeof(int16_t) * 2;
            soundOutput.soundBufSize = soundOutput.samplePerSec * soundOutput.bytesPerSample;
            /*
              TODO(jairo): investigate why Casey was able to use a lower latency (dividing by 15).
              In my computer if I try to divide by 15 the sound starts making clipping/skipping noises.
              If try even lower latencies (dividing by 60), the hardware goes full circle and I get
              back to the situation where there's a latency of 1 second between me touching the controller stick
              and the change in the sound pitch taking place.
            */
            soundOutput.nLatencySamples = soundOutput.samplePerSec / 7;
            win32InitDSound(wnd, soundOutput.samplePerSec, soundOutput.soundBufSize);
            win32ClearSoundBuffer(&soundOutput);
            soundBuf->Play(0, 0, DSBPLAY_LOOPING);
            int16_t *soundSamples = (int16_t*)VirtualAlloc(0, soundOutput.soundBufSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            game_input input[2] = {};
            game_input *newInput = &input[0];
            game_input *oldInput = &input[1];
            while (running) {
                LARGE_INTEGER perfCounter;
                QueryPerformanceCounter(&perfCounter);
                uint64_t cycleCount = __rdtsc();
                while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        OutputDebugString("quitting");
                        running = false;
                    }
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                for (WORD ctrlIdx = 0; ctrlIdx < XUSER_MAX_COUNT; ++ctrlIdx)
                    {
                        game_controller_input *oldController = &oldInput->controllers[ctrlIdx];
                        game_controller_input *newController = &newInput->controllers[ctrlIdx];
                        XINPUT_STATE inputState;
                        DWORD gotInput = XInputGetState(ctrlIdx, &inputState);
                        if (gotInput == ERROR_SUCCESS) {
                            XINPUT_GAMEPAD* pad = &inputState.Gamepad;
                            bool padUp = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                            bool padDown = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                            bool padLeft = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                            bool padRight = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                            bool padStart = (pad->wButtons & XINPUT_GAMEPAD_START);
                            bool padBack = (pad->wButtons & XINPUT_GAMEPAD_BACK);
                            bool padLShoulder = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                            bool padRShoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                            bool padA = (pad->wButtons & XINPUT_GAMEPAD_A);
                            bool padB = (pad->wButtons & XINPUT_GAMEPAD_B);
                            bool padX = (pad->wButtons & XINPUT_GAMEPAD_X);
                            bool padY = (pad->wButtons & XINPUT_GAMEPAD_Y);
                            float X;
                            if (pad->sThumbLX < 0)
                                {
                                    X = pad->sThumbLX / SHRT_MAX;
                                }
                            else
                                {
                                    X = pad->sThumbLX / SHRT_MAX;
                                }
                            float Y;
                            if (pad->sThumbLY < 0)
                                {
                                    Y = (float)pad->sThumbLY / SHRT_MAX;
                                }
                            else
                                {
                                    Y = (float)pad->sThumbLY / SHRT_MAX;
                                }
                            newController->isAnalog = true;
                            newController->startX = oldController->endX;
                            newController->startY = oldController->endY;
                            newController->minX = newController->maxX = newController->endX = X;
                            newController->minY = newController->maxY = newController->endY = Y;
                            win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_A,
                                                  &oldController->down, &newController->down);
                            win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_B,
                                                  &oldController->right, &newController->right);
                            win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_X,
                                                  &oldController->left, &newController->left);
                            win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_Y,
                                                  &oldController->up, &newController->up);
                            win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                  &oldController->leftBumper, &newController->leftBumper);
                            win32ProcessXInputBtn(pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                  &oldController->rightBumper, &newController->rightBumper);
                        }
                        else {
                            // TODO handle controller disconnected
                        }
                    }

                game_offscreen_buffer buf = {};
                buf.memory = backbuffer.memory;
                buf.width  = backbuffer.width;
                buf.height = backbuffer.height;
                buf.pitch  = backbuffer.pitch;


                DWORD byteToLock;
                DWORD targetCursor;
                DWORD bytesToWrite;
                DWORD playCursor;
                DWORD writeCursor;
                bool soundIsValid = false;
                if (SUCCEEDED(soundBuf->GetCurrentPosition(&playCursor, &writeCursor)))
                    {
                        byteToLock = (soundOutput.runningSampleIdx * soundOutput.bytesPerSample) % soundOutput.soundBufSize;
                        targetCursor = (playCursor + soundOutput.nLatencySamples * soundOutput.bytesPerSample)% soundOutput.soundBufSize;
                        bytesToWrite;
                        if (byteToLock > targetCursor)
                            {
                                bytesToWrite = soundOutput.soundBufSize - byteToLock + targetCursor;
                            }
                        else
                            {
                                bytesToWrite = targetCursor - byteToLock;
                            }
                        soundIsValid = true;
                    }
                else
                    {
                        soundIsValid = false;
                    }
                game_sound_buffer sBuf = {};
                sBuf.samplesPerSec = soundOutput.samplePerSec;
                sBuf.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                sBuf.samples = soundSamples;

                gameUpdateAndRender(newInput, &buf, &sBuf);

                if (soundIsValid)
                    {
                        win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &sBuf);
                    }

                win32_window_dimension d = win32GetWindowDimension(wnd);
                /* NOTE(jairo): finally found out that the clipping noise bug is actually caused by this call (but only the window is being shown).
                 apparently, copying the entire screen buffer slows the loop down and we end up being unable
                 to fill the sound buffer quickly enough. */
                // TODO(jairo): improve this. maybe BitBlt?
                // for now, increasing the latency (nLatencySamples) solves the problem
                win32DisplayBufferInWindow(&backbuffer, hdc, d.width, d.height);

                LARGE_INTEGER perfCounterEnd;
                QueryPerformanceCounter(&perfCounterEnd);
                uint64_t cycleCountEnd = __rdtsc();
                int64_t msPerFrame = (1000 * (perfCounterEnd.QuadPart - perfCounter.QuadPart)) / perfFreq.QuadPart;
                DEBUG("DBG perf count: %lld\n", (1000 * (perfCounterEnd.QuadPart - perfCounter.QuadPart)) / perfFreq.QuadPart);
                DEBUG("DBG fps: %lld\n", 1000/msPerFrame);
                DEBUG("DBG cpu cycles elapsed: %lld\n", cycleCountEnd - cycleCount);

                game_input *temp = newInput;
                game_input *newInput = oldInput;
                game_input *oldInput = temp;
            }
        } else {
            //TODO
        }
    } else {
        //TODO()
    }
    return 0;
}
