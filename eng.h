#ifndef ENG_H
#define ENG_H

// Basic types
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef double f64;

// Engine state
typedef struct {
    u8 rn;      // running
    u8 win;     // window created
    void* dpy;  // display
    u32 wid;    // window id
    void* gc;   // graphics context
    u32 lt;     // last time
    u32 ct;     // current time
    u32 dt;     // delta time
    u32 fps;    // frames per second
    u32 fc;     // frame counter
    u32 ft;     // frame time
} eng_t;

// Engine functions
void ini(void);
void run(void);
void fin(void);

#endif
