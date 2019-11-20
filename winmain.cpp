#include <windows.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_var    static

// TODO fix this global
global_var bool running = true;

global_var int BYTES_PER_PIXEL = 4;

global_var BITMAPINFO bitmapInfo;
global_var void* bitmapMemory;
global_var int bitmapWidth;
global_var int bitmapHeight;

internal void
renderWeirdGradient(int xOffset, int yOffset)
{
	int width = bitmapWidth;
	int height = bitmapHeight;
	int pitch = width * BYTES_PER_PIXEL;
	uint8_t *row = (uint8_t*)bitmapMemory;
	for (int y = 0; y < bitmapHeight; ++y) {
		uint32_t* pixel = (uint32_t*)row;
		for (int x = 0; x < bitmapWidth; ++x) {
			/*
			pixel in memory:  00 00 00 00 (four hexadecimal values: B G R padding)
			*/
			uint8_t b = (x + xOffset);
			uint8_t g = (y + yOffset);
			*pixel++ = ((g << 8) | b) ;
		}
		row += pitch;
	}
}

internal void
win32ResizeDIBSection(int width, int height)
{
	if (bitmapMemory) {
		VirtualFree(bitmapMemory, 0, MEM_RELEASE);
	}

	bitmapWidth = width;
	bitmapHeight = height;

	bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
	bitmapInfo.bmiHeader.biWidth = width;
	bitmapInfo.bmiHeader.biHeight = -height;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;

	//after clarification about stretchDIBits and BitBlt, we know
	// we can allocate the memory ourselves
	int bitmapMemorySize = BYTES_PER_PIXEL * bitmapWidth * bitmapHeight;
	bitmapMemory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);


	// TODO maybe clear to black
}

internal void
win32UpdateWindow(HDC hdc, RECT *wndRect, LONG x, LONG y, LONG width, LONG height)
{
	//OutputDebugString(" win32UpdateWindow is actually getting called\n");

	int wndWidth = wndRect->right - wndRect->left;
	int wndHeight = wndRect->bottom - wndRect->top;
	StretchDIBits(
		hdc,
		0, 0, bitmapWidth, bitmapHeight,
		0, 0, wndWidth, wndHeight,
		bitmapMemory,
		&bitmapInfo,
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
			RECT rect;
			GetClientRect(hwnd, &rect);
			int w = rect.right - rect.left;
			int h = rect.bottom - rect.top;

			win32ResizeDIBSection(w, h);
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
            PAINTSTRUCT p;
            HDC devCtx = BeginPaint(hwnd, &p);
			RECT rect;
			GetClientRect(hwnd, &rect);
			LONG x = p.rcPaint.left;
			LONG y = p.rcPaint.top;
            LONG h = p.rcPaint.bottom - p.rcPaint.top;
            LONG w = p.rcPaint.right - p.rcPaint.left;
			win32UpdateWindow(devCtx, &rect, x, y, h, w);
            //PatBlt(devCtx, x, y, w, h, WHITENESS);
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

				renderWeirdGradient(xOffset, yOffset);
				HDC hdc = GetDC(wnd);
				RECT wndRect;
				GetClientRect(wnd, &wndRect);
				int wndWidth = wndRect.right - wndRect.left;
				int wndHeight = wndRect.bottom - wndRect.top;
				win32UpdateWindow(hdc, &wndRect, 0, 0, wndWidth, wndHeight);
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