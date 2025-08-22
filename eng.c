#define _POSIX_C_SOURCE 199309L
#include "eng.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <time.h>
#include <alsa/asoundlib.h>

// Engine state
static eng_t e;

// Get current time in milliseconds
static u32 tm(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// Convert X11 key to engine key code
static u8 xk(KeySym ks)
{
    switch (ks) {
        case XK_Escape: return KEY_ESC;
        case XK_space: return KEY_SPACE;
        case XK_Up: return KEY_UP;
        case XK_Down: return KEY_DOWN;
        case XK_Left: return KEY_LEFT;
        case XK_Right: return KEY_RIGHT;
        default: return 0;
    }
}

// Set graphics context color
static void set_col(col c)
{
    XColor xc;
    Colormap cm = DefaultColormap(e.dpy, DefaultScreen(e.dpy));
    
    xc.red = c.r * 256;
    xc.green = c.g * 256;
    xc.blue = c.b * 256;
    xc.flags = DoRed | DoGreen | DoBlue;
    
    XAllocColor(e.dpy, cm, &xc);
    XSetForeground(e.dpy, e.gc, xc.pixel);
}

// Generate sine wave audio sample
static snd gen_sin(u32 hz, u32 ms, u32 rate)
{
    snd s;
    s.rate = rate;
    s.ch = 1;
    s.len = (rate * ms) / 1000;
    s.data = malloc(s.len * sizeof(s16));
    
    if (!s.data) {
        s.len = 0;
        return s;
    }
    
    for (u32 i = 0; i < s.len; i++) {
        s16 sample = 3000 * sin(2 * M_PI * hz * i / rate);
        s.data[i] = sample;
    }
    
    return s;
}

// Generate square wave audio sample
static snd gen_sqr(u32 hz, u32 ms, u32 rate)
{
    snd s;
    s.rate = rate;
    s.ch = 1;
    s.len = (rate * ms) / 1000;
    s.data = malloc(s.len * sizeof(s16));
    
    if (!s.data) {
        s.len = 0;
        return s;
    }
    
    u32 period = rate / hz;
    for (u32 i = 0; i < s.len; i++) {
        s16 sample = ((i % period) < (period / 2)) ? 8000 : -8000;
        s.data[i] = sample;
    }
    
    return s;
}

// Vector functions implementation
v2 v2_mk(f32 x, f32 y) { v2 r = {x, y}; return r; }
v2 v2_add(v2 a, v2 b) { return v2_mk(a.x + b.x, a.y + b.y); }
v2 v2_sub(v2 a, v2 b) { return v2_mk(a.x - b.x, a.y - b.y); }
v2 v2_mul(v2 a, f32 s) { return v2_mk(a.x * s, a.y * s); }
v2 v2_div(v2 a, f32 s) { return v2_mk(a.x / s, a.y / s); }
f32 v2_len(v2 a) { return sqrtf(a.x * a.x + a.y * a.y); }
v2 v2_nrm(v2 a) { f32 l = v2_len(a); return l > 0 ? v2_div(a, l) : v2_mk(0, 0); }
f32 v2_dot(v2 a, v2 b) { return a.x * b.x + a.y * b.y; }

// Sprite functions implementation
spr spr_mk(v2 pos, v2 sz, col clr)
{
    spr s;
    s.pos = pos;
    s.sz = sz;
    s.clr = clr;
    s.vis = 1;
    return s;
}

void spr_add(spr s)
{
    if (e.ns % 10 == 0) {
        e.sprs = realloc(e.sprs, (e.ns + 10) * sizeof(spr));
    }
    e.sprs[e.ns++] = s;
}

void spr_drw(spr s)
{
    if (!s.vis) return;
    set_col(s.clr);
    XFillRectangle(e.dpy, e.wid, e.gc, 
                  (int)s.pos.x, (int)s.pos.y, 
                  (int)s.sz.x, (int)s.sz.y);
}

u8 spr_col(spr a, spr b)
{
    if (!a.vis || !b.vis) return 0;
    return (a.pos.x < b.pos.x + b.sz.x &&
            a.pos.x + a.sz.x > b.pos.x &&
            a.pos.y < b.pos.y + b.sz.y &&
            a.pos.y + a.sz.y > b.pos.y);
}

// Audio functions implementation
void aud_ini(void)
{
    int err;
    
    // Open PCM device
    err = snd_pcm_open(&e.ahan, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Audio open error: %s\n", snd_strerror(err));
        e.aud = 0;
        return;
    }
    
    // Set parameters
    err = snd_pcm_set_params(e.ahan,
                            SND_PCM_FORMAT_S16_LE,
                            SND_PCM_ACCESS_RW_INTERLEAVED,
                            1, 44100, 1, 50000);
    if (err < 0) {
        fprintf(stderr, "Audio set params error: %s\n", snd_strerror(err));
        snd_pcm_close(e.ahan);
        e.aud = 0;
        return;
    }
    
    // Generate sound samples
    e.snds[SND_JUMP] = gen_sin(440, 200, 44100);   // A4 note
    e.snds[SND_HIT] = gen_sqr(220, 100, 44100);    // A3 note
    e.snds[SND_CLICK] = gen_sqr(880, 50, 44100);   // A5 note
    
    e.aud = 1;
    printf("Audio initialized\n");
}

void aud_play(u8 s)
{
    if (!e.aud || s >= 8 || !e.snds[s].data) return;
    
    int err = snd_pcm_writei(e.ahan, e.snds[s].data, e.snds[s].len);
    if (err < 0) {
        fprintf(stderr, "Audio write error: %s\n", snd_strerror(err));
        snd_pcm_recover(e.ahan, err, 0);
    }
}

void aud_fin(void)
{
    if (!e.aud) return;
    
    // Free sound samples
    for (int i = 0; i < 8; i++) {
        if (e.snds[i].data) {
            free(e.snds[i].data);
        }
    }
    
    snd_pcm_close(e.ahan);
    e.aud = 0;
    printf("Audio shutdown\n");
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
    XSelectInput(e.dpy, e.wid, ExposureMask | KeyPressMask | KeyReleaseMask);
    
    // Map window
    XMapWindow(e.dpy, e.wid);
    
    // Init timing
    e.lt = tm();
    e.fps = 60;
    e.fc = 0;
    e.ft = 1000 / e.fps;
    
    // Clear key states
    for (int i = 0; i < 16; i++) {
        e.keys[i] = 0;
    }
    
    // Init sprite system
    e.sprs = NULL;
    e.ns = 0;
    
    // Create some test sprites
    col red = {255, 0, 0};
    col green = {0, 255, 0};
    col blue = {0, 0, 255};
    
    // Platform
    spr_add(spr_mk(v2_mk(300, 500), v2_mk(200, 20), green));
    
    // Obstacles
    spr_add(spr_mk(v2_mk(100, 400), v2_mk(50, 50), blue));
    spr_add(spr_mk(v2_mk(600, 300), v2_mk(50, 50), blue));
    
    // Init audio
    aud_ini();
    
    e.win = 1;
    e.rn = 1;
    
    printf("Window created\n");
    printf("Sprites initialized: %u\n", e.ns);
}

void run(void)
{
    if (!e.rn) return;
    
    XEvent ev;
    
    // Physics state using vectors
    static v2 pos = {100.0f, 100.0f};
    static v2 vel = {2.0f, 3.0f};
    static v2 acc = {0.0f, 0.1f}; // Gravity
    static u8 was_space = 0;
    
    // Create player sprite
    spr player = spr_mk(pos, v2_mk(50, 50), (col){255, 0, 0});
    
    while (e.rn) {
        // Handle events
        while (XPending(e.dpy)) {
            XNextEvent(e.dpy, &ev);
            
            switch (ev.type) {
                case KeyPress:
                case KeyRelease: {
                    KeySym ks = XLookupKeysym(&ev.xkey, 0);
                    u8 k = xk(ks);
                    if (k) {
                        e.keys[k] = (ev.type == KeyPress);
                        if (k == KEY_ESC && ev.type == KeyPress) {
                            e.rn = 0;
                        }
                        if (k == KEY_SPACE && ev.type == KeyPress) {
                            aud_play(SND_CLICK);
                        }
                    }
                    break;
                }
            }
        }
        
        // Timing
        e.ct = tm();
        e.dt = e.ct - e.lt;
        
        if (e.dt >= e.ft) {
            e.lt = e.ct;
            e.fc++;
            
            // Handle input
            if (e.keys[KEY_UP]) acc.y = -0.2f;
            else if (e.keys[KEY_DOWN]) acc.y = 0.2f;
            else acc.y = 0.1f; // Default gravity
            
            if (e.keys[KEY_LEFT]) acc.x = -0.2f;
            else if (e.keys[KEY_RIGHT]) acc.x = 0.2f;
            else acc.x = 0.0f;
            
            // Jump with sound
            if (e.keys[KEY_SPACE] && !was_space) {
                vel.y = -5.0f;
                aud_play(SND_JUMP);
                was_space = 1;
            } else if (!e.keys[KEY_SPACE]) {
                was_space = 0;
            }
            
            // Physics update
            vel = v2_add(vel, acc);
            pos = v2_add(pos, vel);
            
            // Update player sprite position
            player.pos = pos;
            
            // Check collisions with sprites
            u8 hit = 0;
            for (u32 i = 0; i < e.ns; i++) {
                if (spr_col(player, e.sprs[i])) {
                    hit = 1;
                    // Simple collision response
                    vel.y = -vel.y * 0.8f;
                    
                    // Position correction
                    if (pos.y < e.sprs[i].pos.y) {
                        pos.y = e.sprs[i].pos.y - player.sz.y;
                    } else {
                        pos.y = e.sprs[i].pos.y + e.sprs[i].sz.y;
                    }
                    
                    player.pos = pos;
                }
            }
            
            // Play hit sound
            if (hit) {
                aud_play(SND_HIT);
            }
            
            // Boundary collision
            if (pos.x > 750.0f || pos.x < 0.0f) {
                vel.x = -vel.x * 0.8f;
                if (pos.x > 750.0f) pos.x = 750.0f;
                if (pos.x < 0.0f) pos.x = 0.0f;
                player.pos = pos;
            }
            
            if (pos.y > 550.0f || pos.y < 0.0f) {
                vel.y = -vel.y * 0.8f;
                if (pos.y > 550.0f) pos.y = 550.0f;
                if (pos.y < 0.0f) pos.y = 0.0f;
                player.pos = pos;
            }
            
            // Clear screen
            XClearWindow(e.dpy, e.wid);
            
            // Draw all sprites
            for (u32 i = 0; i < e.ns; i++) {
                spr_drw(e.sprs[i]);
            }
            
            // Draw player
            spr_drw(player);
            
            // Draw FPS counter
            char buf[64];
            snprintf(buf, sizeof(buf), "FPS: %u POS: (%.1f, %.1f) VEL: (%.1f, %.1f)", 
                    e.fps, pos.x, pos.y, vel.x, vel.y);
            set_col((col){0, 0, 0}); // Black text
            XDrawString(e.dpy, e.wid, e.gc, 10, 20, buf, strlen(buf));
            
            // Draw controls info
            XDrawString(e.dpy, e.wid, e.gc, 10, 40, "Arrows: Apply force", 19);
            XDrawString(e.dpy, e.wid, e.gc, 10, 60, "Space: Jump", 11);
            XDrawString(e.dpy, e.wid, e.gc, 10, 80, "ESC: Quit", 9);
            
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
    // Free sprites
    if (e.sprs) {
        free(e.sprs);
        printf("Sprites freed\n");
    }
    
    // Shutdown audio
    aud_fin();
    
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

u8 key(u8 k)
{
    if (k < 16) {
        return e.keys[k];
    }
    return 0;
}
