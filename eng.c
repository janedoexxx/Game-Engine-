#define _POSIX_C_SOURCE 199309L
#define _GNU_SOURCE // For M_PI
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

// Scene functions
static void menu_init(void);
static void menu_upd(void);
static void menu_drw(void);
static void menu_fin(void);

static void game_init(void);
static void game_upd(void);
static void game_drw(void);
static void game_fin(void);

// Particle functions
static part part_mk(v2 pos, v2 vel, col clr, f32 life, u8 type);
static part_emit part_emit_mk(v2 pos, v2 vel_range, f32 life_range, u8 type, u32 rate);
static void part_emit_gen(part_emit* e);

// BMP file header
#pragma pack(push, 1)
typedef struct {
    u16 type;
    u32 size;
    u16 reserved1;
    u16 reserved2;
    u32 offset;
} bmp_hdr;

typedef struct {
    u32 size;
    s32 width;
    s32 height;
    u16 planes;
    u16 bpp;
    u32 compression;
    u32 image_size;
    s32 x_ppm;
    s32 y_ppm;
    u32 colors_used;
    u32 colors_important;
} bmp_info_hdr;
#pragma pack(pop)

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
        case XK_1: return KEY_1;
        case XK_2: return KEY_2;
        case XK_p: return KEY_P;
        case XK_b: return KEY_B;
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
    s.id = 0;
    
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
    s.id = 0;
    
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
    s.id = 0;
    s.tex_id = 0;
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

void spr_drw_tex(spr s)
{
    if (!s.vis || !s.tex_id) return;
    
    tex* t = tex_get(s.tex_id);
    if (!t || !t->loaded) {
        spr_drw(s); // Fallback to solid color
        return;
    }
    
    // Draw texture (pixel by pixel for now)
    for (u32 y = 0; y < (u32)s.sz.y; y++) {
        for (u32 x = 0; x < (u32)s.sz.x; x++) {
            if (s.pos.x + x < 0 || s.pos.x + x >= 800 || 
                s.pos.y + y < 0 || s.pos.y + y >= 600) {
                continue;
            }
            
            // Calculate texture coordinates
            u32 tx = (x * t->w) / (u32)s.sz.x;
            u32 ty = (y * t->h) / (u32)s.sz.y;
            
            // Get pixel color
            col c = t->data[ty * t->w + tx];
            
            // Skip transparent pixels (black for now)
            if (c.r == 0 && c.g == 0 && c.b == 0) continue;
            
            set_col(c);
            XDrawPoint(e.dpy, e.wid, e.gc, (int)(s.pos.x + x), (int)(s.pos.y + y));
        }
    }
}

u8 spr_col(spr a, spr b)
{
    if (!a.vis || !b.vis) return 0;
    return (a.pos.x < b.pos.x + b.sz.x &&
            a.pos.x + a.sz.x > b.pos.x &&
            a.pos.y < b.pos.y + b.sz.y &&
            a.pos.y + a.sz.y > b.pos.y);
}

spr* spr_get(u32 id)
{
    for (u32 i = 0; i < e.ns; i++) {
        if (e.sprs[i].id == id) {
            return &e.sprs[i];
        }
    }
    return NULL;
}

// Texture functions implementation
u32 tex_load(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open texture: %s\n", path);
        return 0;
    }
    
    // Read BMP header
    bmp_hdr hdr;
    if (fread(&hdr, sizeof(bmp_hdr), 1, f) != 1) {
        fclose(f);
        fprintf(stderr, "Failed to read BMP header: %s\n", path);
        return 0;
    }
    
    // Check BMP signature
    if (hdr.type != 0x4D42) { // 'BM'
        fclose(f);
        fprintf(stderr, "Not a BMP file: %s\n", path);
        return 0;
    }
    
    // Read info header
    bmp_info_hdr info;
    if (fread(&info, sizeof(bmp_info_hdr), 1, f) != 1) {
        fclose(f);
        fprintf(stderr, "Failed to read BMP info header: %s\n", path);
        return 0;
    }
    
    // Check if supported format (24-bit uncompressed)
    if (info.bpp != 24 || info.compression != 0) {
        fclose(f);
        fprintf(stderr, "Unsupported BMP format (must be 24-bit uncompressed): %s\n", path);
        return 0;
    }
    
    // Allocate texture
    tex* t = malloc(sizeof(tex));
    if (!t) {
        fclose(f);
        fprintf(stderr, "Failed to allocate texture: %s\n", path);
        return 0;
    }
    
    t->w = info.width;
    t->h = abs(info.height); // Handle flipped BMPs
    t->data = malloc(t->w * t->h * sizeof(col));
    t->loaded = 0;
    
    if (!t->data) {
        fclose(f);
        free(t);
        fprintf(stderr, "Failed to allocate texture data: %s\n", path);
        return 0;
    }
    
    // Seek to pixel data
    fseek(f, hdr.offset, SEEK_SET);
    
    // Read pixel data (BMP stores pixels in BGR format)
    u32 row_size = ((info.width * 3 + 3) / 4) * 4; // BMP rows are padded to 4 bytes
    u8* row = malloc(row_size);
    
    for (s32 y = t->h - 1; y >= 0; y--) {
        if (fread(row, 1, row_size, f) != row_size) {
            free(row);
            fclose(f);
            free(t->data);
            free(t);
            fprintf(stderr, "Failed to read BMP pixel data: %s\n", path);
            return 0;
        }
        
        for (u32 x = 0; x < t->w; x++) {
            u32 idx = y * t->w + x;
            t->data[idx].b = row[x * 3];     // Blue
            t->data[idx].g = row[x * 3 + 1]; // Green
            t->data[idx].r = row[x * 3 + 2]; // Red
        }
    }
    
    free(row);
    fclose(f);
    
    t->loaded = 1;
    printf("Loaded texture: %s (%ux%u)\n", path, t->w, t->h);
    
    // Add to resource manager
    char name[16];
    snprintf(name, sizeof(name), "tex_%u", e.rm.next_id);
    return res_add(t, RES_TEX, name);
}

void tex_drw(u32 id, v2 pos, v2 sz)
{
    tex* t = tex_get(id);
    if (!t || !t->loaded) return;
    
    // Draw texture (pixel by pixel for now)
    for (u32 y = 0; y < (u32)sz.y; y++) {
        for (u32 x = 0; x < (u32)sz.x; x++) {
            if (pos.x + x < 0 || pos.x + x >= 800 || 
                pos.y + y < 0 || pos.y + y >= 600) {
                continue;
            }
            
            // Calculate texture coordinates
            u32 tx = (x * t->w) / (u32)sz.x;
            u32 ty = (y * t->h) / (u32)sz.y;
            
            // Get pixel color
            col c = t->data[ty * t->w + tx];
            
            // Skip transparent pixels (black for now)
            if (c.r == 0 && c.g == 0 && c.b == 0) continue;
            
            set_col(c);
            XDrawPoint(e.dpy, e.wid, e.gc, (int)(pos.x + x), (int)(pos.y + y));
        }
    }
}

tex* tex_get(u32 id)
{
    return (tex*)res_get(id);
}

void tex_free(u32 id)
{
    tex* t = tex_get(id);
    if (t) {
        if (t->data) free(t->data);
        free(t);
    }
    res_del(id);
}

// Particle functions implementation
part part_mk(v2 pos, v2 vel, col clr, f32 life, u8 type)
{
    part p;
    p.pos = pos;
    p.vel = vel;
    p.acc = v2_mk(0, 0);
    p.clr = clr;
    p.life = life;
    p.max_life = life;
    p.type = type;
    p.active = 1;
    return p;
}

part_emit part_emit_mk(v2 pos, v2 vel_range, f32 life_range, u8 type, u32 rate)
{
    part_emit e;
    e.pos = pos;
    e.vel_range = vel_range;
    e.life_range = life_range;
    e.type = type;
    e.active = 1;
    e.rate = rate;
    e.count = 0;
    return e;
}

void part_emit_gen(part_emit* e)
{
    if (!e->active) return;
    
    e->count++;
    if (e->count >= e->rate) {
        e->count = 0;
        
        // Generate random velocity within range
        v2 vel = v2_mk(
            ((f32)rand() / RAND_MAX) * e->vel_range.x * 2 - e->vel_range.x,
            ((f32)rand() / RAND_MAX) * e->vel_range.y * 2 - e->vel_range.y
        );
        
        // Generate random life within range
        f32 life = ((f32)rand() / RAND_MAX) * e->life_range;
        
        // Set color based on type
        col clr;
        switch (e->type) {
            case PART_DUST:
                clr = (col){200, 200, 200}; // Gray
                break;
            case PART_SPARK:
                clr = (col){255, 255, 100}; // Yellow
                break;
            case PART_SMOKE:
                clr = (col){100, 100, 100}; // Dark gray
                break;
            default:
                clr = (col){255, 255, 255}; // White
        }
        
        part_add(e->pos, vel, clr, life, e->type);
    }
}

void part_init(void)
{
    e.parts = NULL;
    e.np = 0;
    e.emits = NULL;
    e.ne = 0;
}

void part_add(v2 pos, v2 vel, col clr, f32 life, u8 type)
{
    if (e.np % 50 == 0) {
        e.parts = realloc(e.parts, (e.np + 50) * sizeof(part));
    }
    e.parts[e.np++] = part_mk(pos, vel, clr, life, type);
}

void part_emit_add(v2 pos, v2 vel_range, f32 life_range, u8 type, u32 rate)
{
    if (e.ne % 5 == 0) {
        e.emits = realloc(e.emits, (e.ne + 5) * sizeof(part_emit));
    }
    e.emits[e.ne++] = part_emit_mk(pos, vel_range, life_range, type, rate);
}

void part_upd(void)
{
    // Update particles
    for (u32 i = 0; i < e.np; i++) {
        if (!e.parts[i].active) continue;
        
        // Apply acceleration (gravity for dust/smoke, none for sparks)
        if (e.parts[i].type != PART_SPARK) {
            e.parts[i].acc.y += 0.05f;
        }
        
        // Update velocity and position
        e.parts[i].vel = v2_add(e.parts[i].vel, e.parts[i].acc);
        e.parts[i].pos = v2_add(e.parts[i].pos, e.parts[i].vel);
        
        // Decrease life
        e.parts[i].life -= 0.016f; // ~60fps
        
        // Deactivate if life is over
        if (e.parts[i].life <= 0) {
            e.parts[i].active = 0;
        }
    }
    
    // Generate particles from emitters
    for (u32 i = 0; i < e.ne; i++) {
        part_emit_gen(&e.emits[i]);
    }
}

void part_drw(void)
{
    for (u32 i = 0; i < e.np; i++) {
        if (!e.parts[i].active) continue;
        
        // Calculate alpha based on life
        f32 alpha = e.parts[i].life / e.parts[i].max_life;
        col c = e.parts[i].clr;
        
        // Adjust color based on alpha
        col draw_col = {
            (u8)(c.r * alpha),
            (u8)(c.g * alpha),
            (u8)(c.b * alpha)
        };
        
        set_col(draw_col); // Fixed: was set_col(ddraw_col);
        
        // Draw different shapes based on type
        switch (e.parts[i].type) {
            case PART_DUST:
                XDrawPoint(e.dpy, e.wid, e.gc, 
                          (int)e.parts[i].pos.x, (int)e.parts[i].pos.y);
                break;
            case PART_SPARK:
                XDrawLine(e.dpy, e.wid, e.gc,
                         (int)e.parts[i].pos.x, (int)e.parts[i].pos.y,
                         (int)(e.parts[i].pos.x - e.parts[i].vel.x),
                         (int)(e.parts[i].pos.y - e.parts[i].vel.y));
                break;
            case PART_SMOKE:
                XFillArc(e.dpy, e.wid, e.gc,
                        (int)(e.parts[i].pos.x - 2), (int)(e.parts[i].pos.y - 2),
                        4, 4, 0, 360*64);
                break;
        }
    }
}


void part_clear(void)
{
    if (e.parts) {
        free(e.parts);
        e.parts = NULL;
        e.np = 0;
    }
    if (e.emits) {
        free(e.emits);
        e.emits = NULL;
        e.ne = 0;
    }
}

// Resource functions implementation
u32 res_add(void* data, u8 type, const char* name)
{
    if (e.rm.nr % 10 == 0) {
        e.rm.ress = realloc(e.rm.ress, (e.rm.nr + 10) * sizeof(res));
    }
    
    res* r = &e.rm.ress[e.rm.nr++];
    r->id = e.rm.next_id++;
    r->type = type;
    r->data = data;
    strncpy(r->name, name, sizeof(r->name) - 1);
    r->name[sizeof(r->name) - 1] = '\0';
    
    return r->id;
}

void* res_get(u32 id)
{
    for (u32 i = 0; i < e.rm.nr; i++) {
        if (e.rm.ress[i].id == id) {
            return e.rm.ress[i].data;
        }
    }
    return NULL;
}

void* res_find(const char* name)
{
    for (u32 i = 0; i < e.rm.nr; i++) {
        if (strcmp(e.rm.ress[i].name, name) == 0) {
            return e.rm.ress[i].data;
        }
    }
    return NULL;
}

void res_del(u32 id)
{
    for (u32 i = 0; i < e.rm.nr; i++) {
        if (e.rm.ress[i].id == id) {
            // Free the resource data
            if (e.rm.ress[i].type == RES_SND) {
                snd* s = (snd*)e.rm.ress[i].data;
                if (s && s->data) {
                    free(s->data);
                }
            } else if (e.rm.ress[i].type == RES_TEX) {
                tex* t = (tex*)e.rm.ress[i].data;
                if (t && t->data) {
                    free(t->data);
                }
            }
            free(e.rm.ress[i].data);
            
            // Move last element to this position
            if (i < e.rm.nr - 1) {
                e.rm.ress[i] = e.rm.ress[e.rm.nr - 1];
            }
            e.rm.nr--;
            return;
        }
    }
}

void res_clear(void)
{
    for (u32 i = 0; i < e.rm.nr; i++) {
        if (e.rm.ress[i].type == RES_SND) {
            snd* s = (snd*)e.rm.ress[i].data;
            if (s && s->data) {
                free(s->data);
            }
        } else if (e.rm.ress[i].type == RES_TEX) {
            tex* t = (tex*)e.rm.ress[i].data;
            if (t && t->data) {
                free(t->data);
            }
        }
        free(e.rm.ress[i].data);
    }
    free(e.rm.ress);
    e.rm.ress = NULL;
    e.rm.nr = 0;
    e.rm.next_id = 1;
}

// Audio functions implementation
void aud_ini(void)
{
    int err;
    
    // Open PCM device
    err = snd_pcm_open((snd_pcm_t**)&e.ahan, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Audio open error: %s\n", snd_strerror(err));
        e.aud = 0;
        return;
    }
    
    // Set parameters
    err = snd_pcm_set_params((snd_pcm_t*)e.ahan,
                            SND_PCM_FORMAT_S16_LE,
                            SND_PCM_ACCESS_RW_INTERLEAVED,
                            1, 44100, 1, 50000);
    if (err < 0) {
        fprintf(stderr, "Audio set params error: %s\n", snd_strerror(err));
        snd_pcm_close((snd_pcm_t*)e.ahan);
        e.aud = 0;
        return;
    }
    
    // Generate sound samples and add to resource manager
    snd* jump_snd = malloc(sizeof(snd));
    *jump_snd = gen_sin(440, 200, 44100);
    res_add(jump_snd, RES_SND, "jump");
    
    snd* hit_snd = malloc(sizeof(snd));
    *hit_snd = gen_sqr(220, 100, 44100);
    res_add(hit_snd, RES_SND, "hit");
    
    snd* click_snd = malloc(sizeof(snd));
    *click_snd = gen_sqr(880, 50, 44100);
    res_add(click_snd, RES_SND, "click");
    
    e.aud = 1;
    printf("Audio initialized\n");
}

void aud_play(u8 s)
{
    if (!e.aud) return;
    
    const char* snd_names[] = {"jump", "hit", "click"};
    if (s >= sizeof(snd_names)/sizeof(snd_names[0])) return;
    
    snd* sound = (snd*)res_find(snd_names[s]);
    if (!sound || !sound->data) return;
    
    int err = snd_pcm_writei((snd_pcm_t*)e.ahan, sound->data, sound->len);
    if (err < 0) {
        fprintf(stderr, "Audio write error: %s\n", snd_strerror(err));
        snd_pcm_recover((snd_pcm_t*)e.ahan, err, 0);
    }
}

void aud_fin(void)
{
    if (!e.aud) return;
    
    snd_pcm_close((snd_pcm_t*)e.ahan);
    e.aud = 0;
    printf("Audio shutdown\n");
}

snd* snd_get(u32 id)
{
    return (snd*)res_get(id);
}

// Scene functions implementation
void scn_add(u32 id, scn_init_fn init, scn_upd_fn upd, scn_drw_fn drw, scn_fin_fn fin)
{
    if (e.sm.ns % 5 == 0) {
        e.sm.scns = realloc(e.sm.scns, (e.sm.ns + 5) * sizeof(scn));
    }
    
    scn* s = &e.sm.scns[e.sm.ns++];
    s->id = id;
    s->init = init;
    s->upd = upd;
    s->drw = drw;
    s->fin = fin;
    s->active = 0;
}

void scn_set(u32 id)
{
    // Deactivate current scene
    if (e.sm.cur < e.sm.ns) {
        scn* cur = &e.sm.scns[e.sm.cur];
        if (cur->fin) cur->fin();
        cur->active = 0;
    }
    
    // Find and activate new scene
    for (u32 i = 0; i < e.sm.ns; i++) {
        if (e.sm.scns[i].id == id) {
            e.sm.cur = i;
            e.sm.scns[i].active = 1;
            if (e.sm.scns[i].init) e.sm.scns[i].init();
            return;
        }
    }
    
    // Fallback to first scene if not found
    if (e.sm.ns > 0) {
        e.sm.cur = 0;
        e.sm.scns[0].active = 1;
        if (e.sm.scns[0].init) e.sm.scns[0].init();
    }
}

void scn_upd(void)
{
    if (e.sm.cur < e.sm.ns && e.sm.scns[e.sm.cur].upd) {
        e.sm.scns[e.sm.cur].upd();
    }
}

void scn_drw(void)
{
    if (e.sm.cur < e.sm.ns && e.sm.scns[e.sm.cur].drw) {
        e.sm.scns[e.sm.cur].drw();
    }
}

// Menu scene implementation
static void menu_init(void)
{
    printf("Menu scene initialized\n");
    part_clear();
}

static void menu_upd(void)
{
    // Switch to game scene on space
    if (key(KEY_SPACE)) {
        scn_set(SCENE_GAME);
        aud_play(SND_CLICK);
    }
}

static void menu_drw(void)
{
    XClearWindow(e.dpy, e.wid);
    
    set_col((col){0, 0, 0});
    XDrawString(e.dpy, e.wid, e.gc, 300, 200, "GAME ENGINE DEMO", 16);
    XDrawString(e.dpy, e.wid, e.gc, 320, 250, "Press SPACE to play", 19);
    XDrawString(e.dpy, e.wid, e.gc, 340, 300, "Press ESC to quit", 17);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "FPS: %u", e.fps);
    XDrawString(e.dpy, e.wid, e.gc, 10, 20, buf, strlen(buf));
}

static void menu_fin(void)
{
    printf("Menu scene finished\n");
}

// Game scene implementation
static v2 pos, vel, acc;
static u8 was_space;
static spr player;
static u8 part_enabled = 1;
static u32 player_tex = 0;

static void game_init(void)
{
    printf("Game scene initialized\n");
    
    // Initialize physics
    pos = v2_mk(100.0f, 100.0f);
    vel = v2_mk(2.0f, 3.0f);
    acc = v2_mk(0.0f, 0.1f);
    was_space = 0;
    
    // Create player sprite
    player = spr_mk(pos, v2_mk(50, 50), (col){255, 0, 0});
    
    // Load player texture
    player_tex = tex_load("player.bmp");
    if (player_tex) {
        player.tex_id = player_tex;
    }
    
    // Initialize particles
    part_init();
    
    // Add particle emitters
    part_emit_add(v2_mk(400, 300), v2_mk(1, 1), 2.0f, PART_DUST, 2);
    part_emit_add(v2_mk(400, 300), v2_mk(0.5, 0.5), 1.5f, PART_SMOKE, 5);
}

static void game_upd(void)
{
    // Toggle particles with P key
    static u8 was_p = 0;
    if (key(KEY_P) && !was_p) {
        part_enabled = !part_enabled;
        was_p = 1;
    } else if (!key(KEY_P)) {
        was_p = 0;
    }
    
    // Toggle textures with B key
    static u8 was_b = 0;
    if (key(KEY_B) && !was_b) {
        e.use_tex = !e.use_tex;
        was_b = 1;
    } else if (!key(KEY_B)) {
        was_b = 0;
    }
    
    // Handle input
    if (key(KEY_UP)) acc.y = -0.2f;
    else if (key(KEY_DOWN)) acc.y = 0.2f;
    else acc.y = 0.1f; // Default gravity
    
    if (key(KEY_LEFT)) acc.x = -0.2f;
    else if (key(KEY_RIGHT)) acc.x = 0.2f;
    else acc.x = 0.0f;
    
    // Jump with sound
    if (key(KEY_SPACE) && !was_space) {
        vel.y = -5.0f;
        aud_play(SND_JUMP);
        was_space = 1;
        
        // Add jump particles
        if (part_enabled) {
            for (int i = 0; i < 10; i++) {
                v2 part_vel = v2_mk(
                    ((f32)rand() / RAND_MAX) * 4 - 2,
                    ((f32)rand() / RAND_MAX) * -3 - 1
                );
                part_add(v2_mk(pos.x + 25, pos.y + 50), part_vel, 
                        (col){255, 255, 100}, 1.0f, PART_SPARK);
            }
        }
    } else if (!key(KEY_SPACE)) {
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
            
            // Add hit particles
            if (part_enabled && hit) {
                for (int j = 0; j < 5; j++) {
                    v2 part_vel = v2_mk(
                        ((f32)rand() / RAND_MAX) * 6 - 3,
                        ((f32)rand() / RAND_MAX) * -4 - 1
                    );
                    part_add(v2_mk(pos.x + 25, pos.y + 25), part_vel, 
                            (col){200, 200, 200}, 0.8f, PART_DUST);
                }
            }
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
    
    // Update particles
    if (part_enabled) {
        part_upd();
    }
    
    // Return to menu on ESC
    if (key(KEY_ESC)) {
        scn_set(SCENE_MENU);
        aud_play(SND_CLICK);
    }
}

static void game_drw(void)
{
    XClearWindow(e.dpy, e.wid);
    
    // Draw all sprites
    for (u32 i = 0; i < e.ns; i++) {
        spr_drw(e.sprs[i]);
    }
    
    // Draw player (with texture if available and enabled)
    if (e.use_tex && player.tex_id) {
        spr_drw_tex(player);
    } else {
        spr_drw(player);
    }
    
    // Draw particles
    if (part_enabled) {
        part_drw();
    }
    
    // Draw FPS counter
    char buf[64];
    snprintf(buf, sizeof(buf), "FPS: %u POS: (%.1f, %.1f) VEL: (%.1f, %.1f)", 
            e.fps, pos.x, pos.y, vel.x, vel.y);
    set_col((col){0, 0, 0}); // Black text
    XDrawString(e.dpy, e.wid, e.gc, 10, 20, buf, strlen(buf));
    
    // Draw resource info
    char res_buf[32];
    snprintf(res_buf, sizeof(res_buf), "Resources: %u", e.rm.nr);
    XDrawString(e.dpy, e.wid, e.gc, 10, 100, res_buf, strlen(res_buf));
    
    // Draw particle info
    char part_buf[32];
    snprintf(part_buf, sizeof(part_buf), "Particles: %u (%s)", e.np, part_enabled ? "ON" : "OFF");
    XDrawString(e.dpy, e.wid, e.gc, 10, 120, part_buf, strlen(part_buf));
    
    // Draw texture info
    char tex_buf[32];
    snprintf(tex_buf, sizeof(tex_buf), "Textures: %s", e.use_tex ? "ON" : "OFF");
    XDrawString(e.dpy, e.wid, e.gc, 10, 140, tex_buf, strlen(tex_buf));
    
    // Draw controls info
    XDrawString(e.dpy, e.wid, e.gc, 10, 40, "Arrows: Apply force", 19);
    XDrawString(e.dpy, e.wid, e.gc, 10, 60, "Space: Jump", 11);
    XDrawString(e.dpy, e.wid, e.gc, 10, 80, "P: Toggle particles", 19);
    XDrawString(e.dpy, e.wid, e.gc, 10, 160, "B: Toggle textures", 18);
    XDrawString(e.dpy, e.wid, e.gc, 10, 180, "ESC: Menu", 9);
}

static void game_fin(void)
{
    printf("Game scene finished\n");
    part_clear();
}

void ini(void)
{
    printf("Engine init\n");
    
    // Initialize resource manager
    e.rm.ress = NULL;
    e.rm.nr = 0;
    e.rm.next_id = 1;
    
    // Initialize scene manager
    e.sm.scns = NULL;
    e.sm.ns = 0;
    e.sm.cur = 0;
    
    // Initialize particle system
    part_init();
    
    // Initialize texture usage
    e.use_tex = 0;
    
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
    
    // Create some test sprites and add to resource manager
    col green = {0, 255, 0};
    col blue = {0, 0, 255};
    
    // Platform
    spr* platform = malloc(sizeof(spr));
    *platform = spr_mk(v2_mk(300, 500), v2_mk(200, 20), green);
    platform->id = res_add(platform, RES_SPR, "platform");
    spr_add(*platform);
    
    // Obstacles
    spr* obstacle1 = malloc(sizeof(spr));
    *obstacle1 = spr_mk(v2_mk(100, 400), v2_mk(50, 50), blue);
    obstacle1->id = res_add(obstacle1, RES_SPR, "obstacle1");
    spr_add(*obstacle1);
    
    spr* obstacle2 = malloc(sizeof(spr));
    *obstacle2 = spr_mk(v2_mk(600, 300), v2_mk(50, 50), blue);
    obstacle2->id = res_add(obstacle2, RES_SPR, "obstacle2");
    spr_add(*obstacle2);
    
    // Init audio
    aud_ini();
    
    // Add scenes
    scn_add(SCENE_MENU, menu_init, menu_upd, menu_drw, menu_fin);
    scn_add(SCENE_GAME, game_init, game_upd, game_drw, game_fin);
    
    // Set initial scene
    scn_set(SCENE_MENU);
    
    e.win = 1;
    e.rn = 1;
    
    printf("Window created\n");
    printf("Sprites initialized: %u\n", e.ns);
    printf("Resources loaded: %u\n", e.rm.nr);
    printf("Scenes loaded: %u\n", e.sm.ns);
}

void run(void)
{
    if (!e.rn) return;
    
    XEvent ev;
    
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
                        if (k == KEY_ESC && ev.type == KeyPress && e.sm.cur == SCENE_MENU) {
                            e.rn = 0;
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
            
            // Update current scene
            scn_upd();
            
            // Draw current scene
            scn_drw();
            
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
    // Finish current scene
    if (e.sm.cur < e.sm.ns && e.sm.scns[e.sm.cur].fin) {
        e.sm.scns[e.sm.cur].fin();
    }
    
    // Free scenes
    if (e.sm.scns) {
        free(e.sm.scns);
        printf("Scenes freed\n");
    }
    
    // Free sprites
    if (e.sprs) {
        free(e.sprs);
        printf("Sprites freed\n");
    }
    
    // Clear particles
    part_clear();
    
    // Clear all resources
    res_clear();
    
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
