#include <windows.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_var    static

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
win32DisplayBufferInWindow(HDC hdc, int wWidth, int wHeight, win32_buffer *buf)
{
	StretchDIBits(
		hdc,
		0, 0, buf->width, buf->height,
		0, 0, wWidth, wHeight,
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
			win32_window_dimension d = win32GetWindowDimension(hwnd);
			win32ResizeDIBSection(&backbuffer, d.width, d.height);
            OutputDebugString("HUEHUSE WM_SIZE\n");
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
			win32DisplayBufferInWindow(devCtx, d.width, d.height, &backbuffer);
            EndPaint(hwnd, &p);
            break;
        }

        default:
        {
            r = DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }

    return r;
}

int CALLBACK
WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        int       nShowCmd)
{

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
            MSG msg;
			int xOffset = 0;
			int yOffset = 0;
            while (running) {
				while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT) {
						running = false;
					}

                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
				}

				renderWeirdGradient(backbuffer, xOffset, yOffset);
				HDC hdc = GetDC(wnd);
				win32_window_dimension d = win32GetWindowDimension(wnd);
				win32DisplayBufferInWindow(hdc, d.width, d.height, &backbuffer);
				ReleaseDC(wnd, hdc);
				++xOffset;
				++yOffset;
            }
        } else {
            //TODO
        }
    } else {
        //TODO()
    }

    return 0;
}