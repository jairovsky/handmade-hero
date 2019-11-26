#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>
#include <cstdio>

#define internal static
#define local_persist static
#define global_var    static

#define PI 3.14159265359f

#define DEBUG(...) {char cad[512]; sprintf(cad, __VA_ARGS__);  OutputDebugString(cad);}

// TODO fix this global
global_var bool running = true;

struct win32_buffer {

	BITMAPINFO info;
	void* memory;
	int width;
	int height;
	int pitch;
	int bytesPerPixel = 4;
};

global_var win32_buffer backbuffer;


internal void
renderWeirdGradient(win32_buffer buf, int xOffset, int yOffset)
{
	uint8_t *row = (uint8_t*)buf.memory;
	for (int y = 0; y < buf.height; ++y) {
		uint32_t* pixel = (uint32_t*)row;
		for (int x = 0; x < buf.width; ++x) {
			/*
			pixel in memory:  00 00 00 00 (four hexadecimal values: B G R padding)
			*/
			uint8_t b = (x + xOffset);
			uint8_t g = (y + yOffset);
			*pixel++ = ((g << 8) | b) ;
		}
		row += buf.pitch;
	}
}

struct win32_window_dimension
{
	int width;
	int height;
};

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


#define DIRECTSOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECTSOUND_CREATE(dsound_create);
DIRECTSOUND_CREATE(DirectSoundCreateStub)
{
	return DSERR_NODRIVER;
}
global_var dsound_create* DirectSoundCreate_ = DirectSoundCreateStub;
#define DirectSoundCreate DirectSoundCreate_

global_var LPDIRECTSOUNDBUFFER soundBuf;

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

#define KeyWasDown(param) ((param & (1 << 30))) != 0
#define KeyIsDown(param) ((param & (1 << 31))) == 0
#define AltIsDown(param) ((param & (1 << 29))) != 0

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

struct win32_sound_output
{
	int samplePerSec;
	int bytesPerSample;
	int soundBufSize;
	int hertz;
	int volume;
	uint32_t runningSampleIdx;
	int wavePeriod;
	float tSine;
	uint32_t nLatencySamples;
};

internal void
win32FillSoundBuffer(win32_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite)
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
		int16_t* sampleOut = (int16_t*)region1;
		for (DWORD i = 0; i < region1SampleCount; ++i)
		{
			float sineValue = sinf(soundOutput->tSine);
			int16_t sampleValue = (int16_t)(sineValue * soundOutput->volume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			soundOutput->tSine += 2.0f * PI / (float)soundOutput->wavePeriod;
			++soundOutput->runningSampleIdx;
		}
		DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
		sampleOut = (int16_t*)region2;
		for (DWORD i = 0; i < region2SampleCount; ++i)
		{
			float sineValue = sinf(soundOutput->tSine);
			int16_t sampleValue = (int16_t)(sineValue * soundOutput->volume);
			*sampleOut++ = sampleValue;
			*sampleOut++ = sampleValue;
			soundOutput->tSine += 2.0f * PI / (float)soundOutput->wavePeriod;
			++soundOutput->runningSampleIdx;
		}
		soundBuf->Unlock(region1, region1Size, region2, region2Size);
	}
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
			int xOffset = 0;
			int yOffset = 0;

			win32_sound_output soundOutput = {};
			soundOutput.samplePerSec = 48000;
			soundOutput.volume = 3000;
			soundOutput.hertz = 256;
			soundOutput.wavePeriod = soundOutput.samplePerSec / soundOutput.hertz;
			soundOutput.bytesPerSample = sizeof(int16_t) * 2;
			soundOutput.soundBufSize = soundOutput.samplePerSec * soundOutput.bytesPerSample;
			/*
				TODO(jairo): investigate why Casey was able to use a lower latency (dividing by 15).
				In my computer if I try to divide by 15 the sound starts making clipping/skipping noises.
				If try even lower latencies (dividing by 60), the hardware goes full circle and I get
				back to the situation where there's a latency of 1 second between me touching the controller stick
				and the change in the sound pitch taking place.
			*/
			soundOutput.nLatencySamples = soundOutput.samplePerSec / 10;
			win32InitDSound(wnd, soundOutput.samplePerSec, soundOutput.soundBufSize);
			win32FillSoundBuffer(&soundOutput, 0, soundOutput.nLatencySamples * soundOutput.bytesPerSample);
			soundBuf->Play(0, 0, DSBPLAY_LOOPING);
            while (running) {
				LARGE_INTEGER perfCounter;
				QueryPerformanceCounter(&perfCounter);
				uint64_t cycleCount = __rdtsc();
				while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT) {
						running = false;
					}
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
				}

				XINPUT_STATE inputState;
				DWORD controller = 0;
				DWORD gotInput = XInputGetState(controller, &inputState);
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
					int16_t stickX = pad->sThumbLX;
					int16_t stickY = pad->sThumbLY;

					soundOutput.hertz = 512 + (int)(256.0f * ((float)stickY / 30000.0f));
					soundOutput.wavePeriod = soundOutput.samplePerSec / soundOutput.hertz;
					if (padUp) { ++yOffset; } if (padDown) { --yOffset; } if (padLeft) { ++xOffset; } if (padRight) { --xOffset; }
				}
				else {
					// TODO handle controller disconnected
				}
				renderWeirdGradient(backbuffer, xOffset, yOffset);

				DWORD playCursor;
				DWORD writeCursor;
				if (SUCCEEDED(soundBuf->GetCurrentPosition(&playCursor, &writeCursor)))
				{
					DWORD byteToLock = (soundOutput.runningSampleIdx * soundOutput.bytesPerSample) % soundOutput.soundBufSize;
					DWORD targetCursor = (playCursor + soundOutput.nLatencySamples * soundOutput.bytesPerSample)% soundOutput.soundBufSize;
					DWORD bytesToWrite;
					if (byteToLock > targetCursor)
					{
						bytesToWrite = soundOutput.soundBufSize - byteToLock + targetCursor;
					}
					else
					{
						bytesToWrite = targetCursor - byteToLock;
					}
					win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite);
				}

				win32_window_dimension d = win32GetWindowDimension(wnd);
				win32DisplayBufferInWindow(&backbuffer, hdc, d.width, d.height);

				LARGE_INTEGER perfCounterEnd;
				QueryPerformanceCounter(&perfCounterEnd);
				uint64_t cycleCountEnd = __rdtsc();
				int64_t msPerFrame = (1000 * (perfCounterEnd.QuadPart - perfCounter.QuadPart)) / perfFreq.QuadPart;
				DEBUG("DBG perf count: %lld\n", (1000 * (perfCounterEnd.QuadPart - perfCounter.QuadPart)) / perfFreq.QuadPart);
				DEBUG("DBG fps: %lld\n", 1000/msPerFrame);
				DEBUG("DBG cpu cycles elapsed: %lld\n", cycleCountEnd - cycleCount);
            }
        } else {
            //TODO
        }
    } else {
        //TODO()
    }
    return 0;
}