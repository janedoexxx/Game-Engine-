#include "eng.h"
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// Engine state
static eng_t e;

void ini(void)
{
    printf("Engine init\n");
    
    // Open display
    e.dpy = XOpenDisplay(NULL);
    if (!e.dpy) {
        fprintf(stderr, "Can't open display\n");
        return;
    }
    
    // Create window
    int s = DefaultScreen(e.dpy);
    e.wid = XCreateSimpleWindow(e.dpy, RootWindow(e.dpy, s),
                               10, 10, 800, 600, 1,
                               BlackPixel(e.dpy, s),
                               WhitePixel(e.dpy, s));
    
    // Set window title
    XStoreName(e.dpy, e.wid, "Game Engine");
    
    // Select events
    XSelectInput(e.dpy, e.wid, ExposureMask | KeyPressMask);
    
    // Map window
    XMapWindow(e.dpy, e.wid);
    
    e.win = 1;
    e.rn = 1;
    
    printf("Window created\n");
}

void run(void)
{
    if (!e.rn) return;
    
    XEvent ev;
    while (e.rn) {
        XNextEvent(e.dpy, &ev);
        
        switch (ev.type) {
            case Expose:
                // Handle expose event
                break;
            case KeyPress:
                // Handle key press
                e.rn = 0;
                break;
        }
    }
}

void fin(void)
{
    if (e.win) {
        XDestroyWindow(e.dpy, e.wid);
        XCloseDisplay(e.dpy);
        printf("Window destroyed\n");
    }
    
    printf("Engine shutdown\n");
    e.rn = 0;
}
