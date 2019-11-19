#include <windows.h>

#define internal static
#define local_persist static
#define global_var    static

// TODO fix this global
global_var bool running = true;
global_var BITMAPINFO bitmapInfo;
global_var void* bitmapMemory;
global_var HBITMAP bitmapHndl;
global_var HDC bitmapDeviceCtx;

internal void
win32ResizeDIBSection(int width, int height)
{

	if (bitmapHndl) {
		DeleteObject(bitmapHndl);
	}

	if (!bitmapDeviceCtx) {
		bitmapDeviceCtx = CreateCompatibleDC(0);
	}
	bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
	bitmapInfo.bmiHeader.biWidth = width;
	bitmapInfo.bmiHeader.biHeight = height;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
//	bitmapInfo.bmiHeader.biSizeImage = 0;
//	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
//	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
//	bitmapInfo.bmiHeader.biClrUsed = 0;
//	bitmapInfo.bmiHeader.biClrImportant = 0;

	bitmapHndl = CreateDIBSection(
		bitmapDeviceCtx,
		&bitmapInfo,
		DIB_RGB_COLORS,
		&bitmapMemory,
		0,
		0);
}

internal void
win32UpdateWindow(HDC hdc, LONG x, LONG y, LONG width, LONG height)
{
	OutputDebugString(" win32UpdateWindow is actually getting called\n");

	StretchDIBits(
		hdc,
		x, y, width, height,
		x, y, width, height,
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
			LONG x = p.rcPaint.left;
			LONG y = p.rcPaint.top;
            LONG h = p.rcPaint.bottom - p.rcPaint.top;
            LONG w = p.rcPaint.right - p.rcPaint.left;
			win32UpdateWindow(devCtx, x, y, h, w);
            PatBlt(devCtx, x, y, w, h, WHITENESS);
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
            while (running) {
                BOOL msgRes = GetMessage(&msg, 0, 0, 0);

                if (msgRes >= 0) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                } else {
                    OutputDebugString("Error receiving msg");
                    break;
                }
            }
        } else {
            //TODO
        }
    } else {
        //TODO()
    }

    return 0;
}