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
    
    // Create graphics context
    XGCValues gv;
    gv.foreground = BlackPixel(e.dpy, s);
    gv.background = WhitePixel(e.dpy, s);
    gv.line_width = 2;
    gv.line_style = LineSolid;
    
    e.gc = XCreateGC(e.dpy, e.wid, 
                    GCForeground | GCBackground | GCLineWidth | GCLineStyle,
                    &gv);
    
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
                // Draw test graphics
                if (e.gc) {
                    XDrawRectangle(e.dpy, e.wid, e.gc, 100, 100, 200, 150);
                    XDrawLine(e.dpy, e.wid, e.gc, 100, 100, 300, 250);
                    XFillArc(e.dpy, e.wid, e.gc, 400, 300, 100, 100, 0, 360*64);
                }
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
    if (e.gc) {
        XFreeGC(e.dpy, e.gc);
        printf("GC freed\n");
    }
    
    if (e.win) {
        XDestroyWindow(e.dpy, e.wid);
        XCloseDisplay(e.dpy);
        printf("Window destroyed\n");
    }
    
    printf("Engine shutdown\n");
    e.rn = 0;
}
