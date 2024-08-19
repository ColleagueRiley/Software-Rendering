# RGFW Under the Hood: Software Rendering
## Introduction
RGFW is a lightweight single-header windowing library, its source code can be found [here](https://github.com/ColleagueRiley/RGFW). 
This tutorial is based on its source code.

The basic idea of software rendering is simple. It comes down to drawing to a buffer and blitting it to the screen.
However, software rendering is a bit more complicated than that when working with low-level APIs. The added complexity is because you must 
properly initialize a rendering context, telling the API how to expect the data. Then to draw you have to use the API's functions to
blit to the screen, which can also be complicated. 

This tutorial explains how RGFW handles software rendering so you can understand how to implement it yourself.

NOTE: MacOS code will be written with a Cocoa C Wrapper in mind (see the RGFW.h or Silicon.h)

## Overview

A quick overview of the steps required

1. Initialize buffer and rendering context
2. Draw to the buffer
3. Blit buffer to the screen 
4. Free leftover data

## Step 1 (Initalize buffer and rendering context)

On X11 you start by creating a Visual (or pixel format) that tells the window how to handle the draw data.
Then create a bitmap for the buffer to render with, RGFW uses an XImage structure for the bitmap. 
Finally, you create a Graphics Context (GC) using the display and window data. The GC is used to tell X11 how to give 
the window its draw data. 

This is also where you can allocate the buffer. The buffer must be allocated for each platform except for Windows. 

For this you need to use, [`XMatchVisualInfo`](https://tronche.com/gui/x/xlib/utilities/XMatchVisualInfo.html), [`XCreateImage`](https://tronche.com/gui/x/xlib/utilities/XCreateImage.html), and [`XCreateGC`](https://www.x.org/releases/X11R7.5/doc/man/man3/XFreeGC.3.html)

```c
XVisualInfo vi;
vi.visual = DefaultVisual(display, DefaultScreen(display));
		
XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vi);

XImage* bitmap = XCreateImage(
			display, XDefaultVisual(display, vi.screen),
			vi.depth,
			ZPixmap, 0, NULL, RGFW_bufferSize.w, RGFW_bufferSize.h,
		    	32, 0
);

/* ..... */
/* Now this visual can be used to create a window and colormap */

XSetWindowAttributes swa;
Colormap cmap;

swa.colormap = cmap = XCreateColormap((Display*) display, DefaultRootWindow(display), vi.visual, AllocNone);

swa.background_pixmap = None;
swa.border_pixel = 0;
swa.event_mask = event_mask;

swa.background_pixel = 0;

Window window = XCreateWindow((Display*) display, DefaultRootWindow((Display*) display), x, y, w, h,
				0, vi.depth, InputOutput, vi.visual,
				CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &swa);
/* .... */

GC gc = XCreateGC(display, window, 0, NULL);

u8* buffer = (u8*)malloc(RGFW_bufferSize.w * RGFW_bufferSize.h * 4);
```

On Windows, you'll start by creating a bitmap header, which is used to create a bitmap with a specified format.
The format structure is used to tell the Windows API how to render the buffer to the screen.

Finally, you create a Drawing Context Handle (HDC) allocated in memory, this is used for selecting the bitmap later.

NOTE: Windows does not need to allocate a buffer because Winapi handles that memory for us. You can also allocate the memory by hand. 

Relevant Documentation: [`BITMAPV5HEADER`](bitmapv5header), [`CreateDIBSection`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection) and [`CreateCompatibleDC`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createcompatibledc)

```c
BITMAPV5HEADER bi = { 0 };
ZeroMemory(&bi, sizeof(bi));
bi.bV5Size = sizeof(bi);
bi.bV5Width = RGFW_bufferSize.w;
bi.bV5Height = -((LONG) RGFW_bufferSize.h);
bi.bV5Planes = 1;
bi.bV5BitCount = 32;
bi.bV5Compression = BI_BITFIELDS;

// where it can expect to find the RGBA data
// (note: this might need to be changed according to the endianness) 
bi.bV5BlueMask = 0x00ff0000;
bi.bV5GreenMask = 0x0000ff00;
bi.bV5RedMask = 0x000000ff;
bi.bV5AlphaMask = 0xff000000;

u8* buffer;

HBITMAP bitmap = CreateDIBSection(hdc,
	(BITMAPINFO*) &bi,
	DIB_RGB_COLORS,
	(void**) &buffer,
	NULL,
	(DWORD) 0);

HDC hdcMem = CreateCompatibleDC(hdc);
```

On MacOS, there is not much setup, most of the work is done during rendering. 

You only need to allocate the buffer data.
```c
u8* buffer = malloc(RGFW_bufferSize.w * RGFW_bufferSize.h * 4);
```

## Step 4 (Draw to the buffer)
For this tutorial, I will use [Silk.h](https://github.com/itsYakub/Silk/) for drawing to the buffer. Silk.h is a single-header software rendering graphics library.


First, include silk, 

```c
#define SILK_PIXELBUFFER_WIDTH w
#define SILK_PIXELBUFFER_HEIGHT h
#define SILK_IMPLEMENTATION
#include "silk.h"
```

Now you can render using silk.

```c
silkClearPixelBufferColor((pixel*)buffer, 0x11AA0033);

silkDrawCircle(
            (pixel*)buffer, 
            (vec2i) { SILK_PIXELBUFFER_WIDTH, SILK_PIXELBUFFER_HEIGHT },
            SILK_PIXELBUFFER_WIDTH,
            (vec2i) { SILK_PIXELBUFFER_CENTER_X, SILK_PIXELBUFFER_CENTER_Y - 60}, 
            60,
            0xff0000ff
);
```

## Step 3 (Blit the buffer to the screen)

On X11, you first set the bitmap data to the buffer.
The bitmap data will be rendered using BGR, so you must  
convert the data if you want to yse RGB. Then you'll have to use `XPutImage`
to draw the XImage to the window using the GC.

Relevant documentation: [`XPutImage`](https://tronche.com/gui/x/xlib/graphics/XPutImage.html)

```c
bitmap->data = (char*) buffer;
#ifndef RGFW_X11_DONT_CONVERT_BGR
	u32 x, y;
	for (y = 0; y < (u32)win->r.h; y++) {
		for (x = 0; x < (u32)win->r.w; x++) {
			u32 index = (y * 4 * area.w) + x * 4;

			u8 red = bitmap->data[index];
			bitmap->data[index] = buffer[index + 2];
			bitmap->data[index + 2] = red;
		}
    }
#endif	
XPutImage(display, (Window)window, gc, bitmap, 0, 0, 0, 0, RGFW_bufferSize.w, RGFW_bufferSize.h);
```


On Windows, you must first select the bitmap and make sure that you save the last selected object so you can reselect it later.
Now you can blit the bitmap to the screen and reselect the old bitmap. 

Relevant documentation: [`SelectObject`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-selectobject) and [`BitBlt`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-bitblt)

```c
HGDIOBJ oldbmp = SelectObject(hdcMem, bitmap);
BitBlt(hdc, 0, 0, win->r.w, win->r.h, hdcMem, 0, 0, SRCCOPY);
SelectObject(hdcMem, oldbmp);
```

On MacOS, set the view's layer according to your window, create a bitmap using the buffer, add the bitmap to the graphics context, and finally draw and flush the context. 

Relevant documentation: [`CGColorSpaceCreateDeviceRGB`](https://developer.apple.com/documentation/coregraphics/1408837-cgcolorspacecreatedevicergb), [`CGBitmapContextCreate`](https://developer.apple.com/documentation/coregraphics/1455939-cgbitmapcontextcreate),  [`CGBitmapContextCreateImage`](https://developer.apple.com/documentation/coregraphics/1454225-cgbitmapcontextcreateimage), [`CGColorSpaceRelease`](https://developer.apple.com/documentation/coregraphics/1408855-cgcolorspacerelease), [`CGContextRelease`](https://developer.apple.com/documentation/coregraphics/1586509-cgcontextrelease),
[`CALayer`](https://developer.apple.com/documentation/quartzcore/calayer?ref=weekly.elfitz.com), [`NSGraphicsContext`](https://developer.apple.com/documentation/appkit/nsgraphicscontext), [`CGContextDrawImage`](https://developer.apple.com/documentation/coregraphics/1454845-cgcontextdrawimage), [`flushGraphics`](https://developer.apple.com/documentation/appkit/nsgraphicscontext/1527919-flushgraphics) and, [`CGImageRelease`](https://developer.apple.com/documentation/coregraphics/1556742-cgimagerelease)
```c
CGImageRef createImageFromBytes(unsigned char *buffer, int width, int height) {
	// Define color space
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	// Create bitmap context
	CGContextRef context = CGBitmapContextCreate(
			buffer, 
			width, height,
			8,
			RGFW_bufferSize.w * 4, 
			colorSpace,
			kCGImageAlphaPremultipliedLast);
	
	// Create image from bitmap context
	CGImageRef image = CGBitmapContextCreateImage(context);
	// Release the color space and context
	CGColorSpaceRelease(colorSpace);
	CGContextRelease(context);
			 
	return image;
}

...
void* view = NSWindow_contentView(window);
void* layer = objc_msgSend_id(view, sel_registerName("layer"));

((void(*)(id, SEL, NSRect))objc_msgSend)(layer,
				sel_registerName("setFrame:"),
				(NSRect){{0, 0}, {win->r.w, win->r.h}});

CGImageRef image = createImageFromBytes(buffer, win->r.w, win->r.h);

// Get the current graphics context
id graphicsContext = objc_msgSend_class(objc_getClass("NSGraphicsContext"), sel_registerName("currentContext"));

// Get the CGContext from the current NSGraphicsContext
id cgContext = objc_msgSend_id(graphicsContext, sel_registerName("graphicsPort"));

// Draw the image in the context
NSRect bounds = (NSRect){{0,0}, {win->r.w, win->r.h}};
CGContextDrawImage((void*)cgContext, *(CGRect*)&bounds, image);

// Flush the graphics context to ensure the drawing is displayed
objc_msgSend_id(graphicsContext, sel_registerName("flushGraphics"));
            
objc_msgSend_void_id(layer, sel_registerName("setContents:"), (id)image);
objc_msgSend_id(layer, sel_registerName("setNeedsDisplay"));
            
CGImageRelease(image);
```
## Step 4 (Free leftover data)

Now you have to free the bitmap and image data using their respective functions.

On X11 and MacOS, you also should free the buffer.

On X11 you must use [`XDestoryImage`](https://tronche.com/gui/x/xlib/utilities/XDestroyImage.html) and [`XFreeGC`](https://tronche.com/gui/x/xlib/GC/XFreeGC.html).
```c
XDestroyImage(bitmap);
XFreeGC(display, gc);
free(buffer);
```

On Windows, you must use [`DeleteDC`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-deletedc) and [`DeleteObject`](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-deleteobject).
```c
DeleteDC(hdcMem);
DeleteObject(bitmap);
```

On MacOS you must use [`release`](https://developer.apple.com/documentation/objectivec/1418956-nsobject/1571957-release).

```c
release(bitmap);
release(image);
free(buffer);
```


## full examples

## X11

```c
// This can be compiled with 
// gcc x11.c -lX11 -lm

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdio.h>
#include <stdlib.h>


#define SILK_PIXELBUFFER_WIDTH 500
#define SILK_PIXELBUFFER_HEIGHT 500
#define SILK_IMPLEMENTATION
#include "silk.h"

int main() {
	Display* display = XOpenDisplay(NULL);
	XVisualInfo vi;
	vi.visual = DefaultVisual(display, DefaultScreen(display));
		
	XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vi);

	XImage* bitmap = XCreateImage(
			display, XDefaultVisual(display, vi.screen),
			vi.depth,
			ZPixmap, 0, NULL, 500, 500,
		    32, 0
	);

	/* ..... */
	/* Now this visual can be used to create a window and colormap */
	
	XSetWindowAttributes swa;
	Colormap cmap;

	swa.colormap = cmap = XCreateColormap((Display*) display, DefaultRootWindow(display), vi.visual, AllocNone);

	swa.background_pixmap = None;
	swa.border_pixel = 0;
	swa.event_mask = CWColormap | CWBorderPixel | CWBackPixel | CWEventMask;

	swa.background_pixel = 0;

	Window window = XCreateWindow((Display*) display, DefaultRootWindow((Display*) display), 500, 500, 500, 500,
					0, vi.depth, InputOutput, vi.visual,
					CWColormap | CWBorderPixel | CWBackPixel | CWEventMask, &swa);
	/* .... */

	GC gc = XCreateGC(display, window, 0, NULL);

	u8* buffer = (u8*)malloc(500 * 500 * 4);

	XSelectInput(display, window, ExposureMask | KeyPressMask);
	XMapWindow(display, window);

	XEvent event;
	for (;;) {
		XNextEvent(display, &event);
		
		silkClearPixelBufferColor((pixel*)buffer, 0x11AA0033);

		silkDrawCircle(
				(pixel*)buffer, 
				(vec2i) { SILK_PIXELBUFFER_WIDTH, SILK_PIXELBUFFER_HEIGHT },
				SILK_PIXELBUFFER_WIDTH,
				(vec2i) { SILK_PIXELBUFFER_CENTER_X, SILK_PIXELBUFFER_CENTER_Y - 60}, 
				60,
				0xff0000ff
		);

		bitmap->data = (char*) buffer;
		#ifndef RGFW_X11_DONT_CONVERT_BGR
			u32 x, y;
			for (y = 0; y < (u32)500; y++) {
				for (x = 0; x < (u32)500; x++) {
					u32 index = (y * 4 * 500) + x * 4;
		
					u8 red = bitmap->data[index];
					bitmap->data[index] = buffer[index + 2];
					bitmap->data[index + 2] = red;
				}
			}
		#endif	
		XPutImage(display, (Window) window, gc, bitmap, 0, 0, 0, 0, 500, 500);
	}

	XDestroyImage(bitmap);
	XFreeGC(display, gc);
	free(buffer);
}
```

## windows

```c
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

    	// where it can expect to find the RGB data
	// (note: this might need to be changed according to the endianness) 
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
```
