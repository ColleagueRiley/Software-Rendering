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
