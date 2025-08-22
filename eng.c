#define _POSIX_C_SOURCE 199309L
#include "eng.h"
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/time.h>
#include <time.h>

// Engine state
static eng_t e;

// Get current time in milliseconds
static u32 tm(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

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
    
    // Init timing
    e.lt = tm();
    e.fps = 60;
    e.fc = 0;
    e.ft = 1000 / e.fps;
    
    e.win = 1;
    e.rn = 1;
    
    printf("Window created\n");
}

void run(void)
{
    if (!e.rn) return;
    
    XEvent ev;
    static u32 x = 100;
    static u32 y = 100;
    static u32 dx = 2;
    static u32 dy = 3;
    
    while (e.rn) {
        // Handle events
        while (XPending(e.dpy)) {
            XNextEvent(e.dpy, &ev);
            
            switch (ev.type) {
                case KeyPress:
                    e.rn = 0;
                    break;
            }
        }
        
        // Timing
        e.ct = tm();
        e.dt = e.ct - e.lt;
        
        if (e.dt >= e.ft) {
            e.lt = e.ct;
            e.fc++;
            
            // Clear screen
            XClearWindow(e.dpy, e.wid);
            
            // Update animation
            x += dx;
            y += dy;
            
            if (x > 700 || x < 100) dx = -dx;
            if (y > 500 || y < 100) dy = -dy;
            
            // Draw moving object
            XFillArc(e.dpy, e.wid, e.gc, x, y, 50, 50, 0, 360*64);
            
            // Draw FPS counter
            char buf[16];
            snprintf(buf, sizeof(buf), "FPS: %u", e.fps);
            XDrawString(e.dpy, e.wid, e.gc, 10, 20, buf, strlen(buf));
            
            // Calculate actual FPS every second
            if (e.fc % 60 == 0) {
                u32 ct = tm();
                u32 elapsed = ct - (e.lt - e.dt);
                if (elapsed > 0) {
                    e.fps = (60 * 1000) / elapsed;
                }
            }
        }
        
        // Small sleep to prevent CPU hogging
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000; // 1 millisecond
        nanosleep(&ts, NULL);
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
