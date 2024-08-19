# RGFW Under the Hood: Software Rendering
## Introduction
RGFW is a lightweight single-header windowing library, its source code can be found [here](https://github.com/ColleagueRiley/RGFW). 
This tutorial is based on its source code.

Basic software rendering is very simple. It comes down to drawing to a buffer and blitting it to the screen.
However it is a bit more complicated than that when you're working with low level apis. This is because you must 
properly initalize a rendering context, telling the api how to expect the data. Then you must use the api's functions to
blit to the screen, which can also be complicated. 

This tutorial explains how RGFW handles software rendering so you can understand how to implement it yourself.

## Overview

A quick overview of the steps required

1. Init buffer and rendering context
2. Draw to the buffer
3. Blit buffer to screen 
4. Free leftover data

## Step 1 (Init buffer and rendering context)

On X11 you start by creating a Visual (or pixel format) that tells the window how to handle the draw data.
Then you must create a bitmap for the buffer to render with, RGFW uses a XImage structure for this. 
Finally, you create a Graphics Context (GC) using the display and window data. The GC is used to tell X11 how to give 
the window its draw data. 

This is also where you can allocate the buffer. The buffer must be allocated for each platform except for windows. 

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

On winmdows you'll start by creating a bitmap header, this is used to create a bitmap with a specifed format.
The format is used to tell the windows api how to render the buffer to the screen.

Finally, you create a Drawing Context Handle (HDC) allocated in memory, this is used for selecting the bitmap later.

NOTE: windows does not need to allocate a buffer because winapi handles that memory for us. You can also allocate the memory by hand. 

windows
```c
	BITMAPV5HEADER bi = { 0 };
	ZeroMemory(&bi, sizeof(bi));
	bi.bV5Size = sizeof(bi);
	bi.bV5Width = RGFW_bufferSize.w;
	bi.bV5Height = -((LONG) RGFW_bufferSize.h);
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
    
	bitmap = CreateDIBSection(.hdc,
		(BITMAPINFO*) &bi,
		DIB_RGB_COLORS,
		(void**) &buffer,
		NULL,
		(DWORD) 0);
	
	hdcMem = CreateCompatibleDC(.hdc);
```

On MacOS, there is not much setup, most of the work is done during rendering. 

You only need to allocate the buffer data.

macos
```c
u8* buffer = malloc(RGFW_bufferSize.w * RGFW_bufferSize.h * 4);
```

## Step 3 (Blit the buffer to the screen)

On X11, you first set the bitmap data to the buffer.
The bitmap data will be rendered using BGR, so you must  
convert the data if you want to. Then you'll have to use `XPutImage`
to draw the XImage to the window using the GC.


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
XPutImage(display, (Window) .window, win->src.gc, win->src.bitmap, 0, 0, 0, 0, RGFW_bufferSize.w, RGFW_bufferSize.h);
```


On windows, you must first select the bitmap, make sure you save the previous selected bitmap so you can reselect it later.
Now, you can blit the bitmap to the screen and reselect the old bitmap. 

```c
HGDIOBJ oldbmp = SelectObject(hdcMem, .bitmap);
BitBlt(hdc, 0, 0, win->r.w, win->r.h, .hdcMem, 0, 0, SRCCOPY);
SelectObject(hdcMem, oldbmp);
```

On MacOS do this : 

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


## Step 3 (Free leftover data)

Now you have to free the bitmap and image data on using their respective function

On X11 and MacOS, you also should free the buffer.

On X11 you must use XDeostyImage and XFreeGC.
```c
XDestroyImage(bitmap);
XFreeGC(display, gc);
free(buffer);
```

On Windows you must use DeleteDC and DeleteObject.
```c
DeleteDC(hdcMem);
DeleteObject(bitmap);
```

On MacOS you must use release.

```c
release(bitmap);
release(image);
free(buffer);
```


## full examples

## X11

```c
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdio.h>
#include <stdlib.h>

typedef unsigned char u8;
typedef unsigned int u32;

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
		
		u32 x, y;
		for (y = 0; y < (u32)500; y++) {
			for (x = 0; x < (u32)500; x++) {
				u32 index = (y * 4 * 500) + x * 4;
				buffer[index] = 0xFF;
				buffer[index + 1] = 0x00;
				buffer[index + 2] = 0x00;
				buffer[index + 3] = 0xFF;
			}
		}

		bitmap->data = (char*) buffer;
		#ifndef RGFW_X11_DONT_CONVERT_BGR
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
