// This can be compiled with 
// gcc win32.c -lgdi32 -lm

#include <windows.h>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#define SILK_PIXELBUFFER_WIDTH 500
#define SILK_PIXELBUFFER_HEIGHT 500
#define SILK_IMPLEMENTATION
#include "silk.h"

int main() {
	WNDCLASS wc = {0};
	wc.lpfnWndProc   = DefWindowProc; // Default window procedure
	wc.hInstance     = GetModuleHandle(NULL);
	wc.lpszClassName = "SampleWindowClass";
	
	RegisterClass(&wc);
	
	HWND hwnd = CreateWindowA(wc.lpszClassName, "Sample Window", 0,
			500, 500, 500, 500,
			NULL, NULL, wc.hInstance, NULL);


	BITMAPV5HEADER bi = { 0 };
	ZeroMemory(&bi, sizeof(bi));
	bi.bV5Size = sizeof(bi);
	bi.bV5Width = 500;
	bi.bV5Height = -((LONG) 500);
	bi.bV5Planes = 1;
	bi.bV5BitCount = 32;
	bi.bV5Compression = BI_BITFIELDS;

    // where it can expect to find the rgba data
    // (note : this might need to be changed according to the edianness) 
	bi.bV5BlueMask = 0x00ff0000;
	bi.bV5GreenMask = 0x0000ff00;
	bi.bV5RedMask = 0x000000ff;
	bi.bV5AlphaMask = 0xff000000;

    u8* buffer;
    
	HDC hdc = GetDC(hwnd); 
	HBITMAP bitmap = CreateDIBSection(hdc,
		(BITMAPINFO*) &bi,
		DIB_RGB_COLORS,
		(void**) &buffer,
		NULL,
		(DWORD) 0);
	
	HDC hdcMem = CreateCompatibleDC(hdc);	

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	
	MSG msg;
	
	BOOL running = TRUE;
	
	while (running) {
		if (PeekMessageA(&msg, hwnd, 0u, 0u, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		running = IsWindow(hwnd);

		silkClearPixelBufferColor((pixel*)buffer, 0x11AA0033);

		silkDrawCircle(
            (pixel*)buffer, 
            (vec2i) { SILK_PIXELBUFFER_WIDTH, SILK_PIXELBUFFER_HEIGHT },
            SILK_PIXELBUFFER_WIDTH,
            (vec2i) { SILK_PIXELBUFFER_CENTER_X, SILK_PIXELBUFFER_CENTER_Y - 60}, 
            60,
            0xff0000ff
		);

		HGDIOBJ oldbmp = SelectObject(hdcMem, bitmap);
		BitBlt(hdc, 0, 0, 500, 500, hdcMem, 0, 0, SRCCOPY);
		SelectObject(hdcMem, oldbmp);
	}

	DeleteDC(hdcMem);
	DeleteObject(bitmap);
	return 0;
}
