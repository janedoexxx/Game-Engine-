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

// Key codes
#define KEY_ESC 0x01
#define KEY_SPACE 0x02
#define KEY_UP 0x03
#define KEY_DOWN 0x04
#define KEY_LEFT 0x05
#define KEY_RIGHT 0x06
#define KEY_1 0x07
#define KEY_2 0x08
#define KEY_P 0x09
#define KEY_B 0x0A
#define KEY_F 0x0B

// Mouse codes
#define MOUSE_LEFT 0x0C
#define MOUSE_MIDDLE 0x0D
#define MOUSE_RIGHT 0x0E

// Audio codes
#define SND_JUMP 0x01
#define SND_HIT 0x02
#define SND_CLICK 0x03

// Resource types
#define RES_SPR 0x01
#define RES_SND 0x02
#define RES_TEX 0x03
#define RES_FONT 0x04

// Scene types
#define SCENE_MENU 0x01
#define SCENE_GAME 0x02

// Particle types
#define PART_DUST 0x01
#define PART_SPARK 0x02
#define PART_SMOKE 0x03

// Font alignment
#define FONT_LEFT 0x01
#define FONT_CENTER 0x02
#define FONT_RIGHT 0x03

// Vector types
typedef struct { f32 x, y; } v2;
typedef struct { f32 x, y, z; } v3;
typedef struct { f32 x, y, z, w; } v4;

// Color type
typedef struct { u8 r, g, b; } col;

// Sprite type
typedef struct {
    v2 pos;
    v2 sz;
    col clr;
    u8 vis;
    u32 id;
    u32 tex_id; // Texture ID
} spr;

// Texture type
typedef struct {
    u32 id;
    u32 w;
    u32 h;
    col* data;
    u8 loaded;
} tex;

// Font character data
typedef struct {
    u8 w;
    u8 h;
    u8* data;
} font_char;

// Font type
typedef struct {
    u32 id;
    u8 cw;
    u8 ch;
    u8 first_char;
    u8 num_chars;
    font_char* chars;
    u8 loaded;
} font;

// Particle type
typedef struct {
    v2 pos;
    v2 vel;
    v2 acc;
    col clr;
    f32 life;
    f32 max_life;
    u8 type;
    u8 active;
} part;

// Particle emitter type
typedef struct {
    v2 pos;
    v2 vel_range;
    f32 life_range;
    u8 type;
    u8 active;
    u32 rate;
    u32 count;
} part_emit;

// Audio sample type
typedef struct {
    s16* data;
    u32 len;
    u32 rate;
    u8 ch;
    u32 id;
} snd;

// Resource entry
typedef struct {
    u32 id;
    u8 type;
    void* data;
    char name[16];
} res;

// Resource manager
typedef struct {
    res* ress;
    u32 nr;
    u32 next_id;
} res_mgr;

// Scene functions
typedef void (*scn_init_fn)(void);
typedef void (*scn_upd_fn)(void);
typedef void (*scn_drw_fn)(void);
typedef void (*scn_fin_fn)(void);

// Scene
typedef struct {
    u32 id;
    scn_init_fn init;
    scn_upd_fn upd;
    scn_drw_fn drw;
    scn_fin_fn fin;
    u8 active;
} scn;

// Scene manager
typedef struct {
    scn* scns;
    u32 ns;
    u32 cur;
} scn_mgr;

// Engine state
typedef struct {
    u8 rn;      // running
    u8 win;     // window created
    u8 aud;     // audio initialized
    void* dpy;  // display
    u32 wid;    // window id
    void* gc;   // graphics context
    u32 lt;     // last time
    u32 ct;     // current time
    u32 dt;     // delta time
    u32 fps;    // frames per second
    u32 fc;     // frame counter
    u32 ft;     // frame time
    u8 keys[16]; // key states
    u8 mouse_btns[3]; // mouse button states
    v2 mouse_pos;     // mouse position
    spr* sprs;  // sprite array
    u32 ns;     // number of sprites
    part* parts; // particles array
    u32 np;     // number of particles
    part_emit* emits; // particle emitters
    u32 ne;     // number of emitters
    void* ahan; // audio handle
    res_mgr rm; // resource manager
    scn_mgr sm; // scene manager
    u8 use_tex; // use textures
    u32 def_font; // default font ID
} eng_t;

// Engine functions
void ini(void);
void run(void);
void fin(void);
u8 key(u8 k);
u8 mouse_btn(u8 btn);
v2 v2_mouse(void);

// Vector functions
v2 v2_mk(f32 x, f32 y);
v2 v2_add(v2 a, v2 b);
v2 v2_sub(v2 a, v2 b);
v2 v2_mul(v2 a, f32 s);
v2 v2_div(v2 a, f32 s);
f32 v2_len(v2 a);
v2 v2_nrm(v2 a);
f32 v2_dot(v2 a, v2 b);

// Sprite functions
spr spr_mk(v2 pos, v2 sz, col clr);
void spr_add(spr s);
void spr_drw(spr s);
void spr_drw_tex(spr s);
u8 spr_col(spr a, spr b);
spr* spr_get(u32 id);

// Texture functions
u32 tex_load(const char* path);
void tex_drw(u32 id, v2 pos, v2 sz);
tex* tex_get(u32 id);
void tex_free(u32 id);

// Font functions
u32 font_load(const char* path, u8 cw, u8 ch, u8 first_char);
void font_drw(u32 id, const char* text, v2 pos, col clr, u8 align);
void font_free(u32 id);
font* font_get(u32 id);
u32 font_text_width(u32 id, const char* text);

// Particle functions
void part_init(void);
void part_add(v2 pos, v2 vel, col clr, f32 life, u8 type);
void part_emit_add(v2 pos, v2 vel_range, f32 life_range, u8 type, u32 rate);
void part_upd(void);
void part_drw(void);
void part_clear(void);

// Audio functions
void aud_ini(void);
void aud_play(u8 s);
void aud_fin(void);
snd* snd_get(u32 id);

// Resource functions
u32 res_add(void* data, u8 type, const char* name);
void* res_get(u32 id);
void* res_find(const char* name);
void res_del(u32 id);
void res_clear(void);

// Scene functions
void scn_add(u32 id, scn_init_fn init, scn_upd_fn upd, scn_drw_fn drw, scn_fin_fn fin);
void scn_set(u32 id);
void scn_upd(void);
void scn_drw(void);

#endif
