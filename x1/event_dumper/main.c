/* ===========================================================================
 * event_dumper — LVGL running as an app on top of the Brain's PEG kernel.
 *
 * Architecture (iter 26, confirmed on real hardware):
 *   - We register as a PegDecoratedWindow with Message override (slot 19).
 *   - Draw dispatch (slot 12) is never invoked for us by PEG's dirty-region
 *     walker, so we push our own shadow buffer to the screen ourselves
 *     via push_shadow_to_screen(), driven from Message and a 200ms LVGL
 *     timer. That guarantees updates without depending on PEG's Invalidate.
 * ========================================================================= */

#include "lvgl/lvgl.h"
#include "pseudo_peg_lib.h"

/* libc stubs supplied by libbrain (see libbrain/sys.s). Extern-declared
 * instead of `#include "libbrain/libbrain.h"` because that header defines
 * `typedef unsigned long size_t` which conflicts with LVGL's `unsigned int`. */
extern void* malloc(unsigned int size);
extern void* realloc(void* ptr, unsigned int size);
extern int free(void* ptr);
extern void* memset(void* ptr, int ch, unsigned int n);

static PegBitmap g_lvgl_pegbmp;

typedef struct {
    void* reserved;
    char* exec_path;
    void* pPresent;
    void* reserved2;
} app_ctx;

/* ===== State ===== */
static void* g_window;
static int32_t g_fb_w = 480;
static int32_t g_fb_h = 640;

/* Saved from app_ctx so app_exit() can hand it back to sh_ExitApp */
static char* g_exec_path;

/* Key code (PegMessage.Param) that triggers a clean exit. Adjust after
 * seeing the `key param=0x??` value in the debug label for the physical
 * button you want to use. */
#define EXIT_KEY_SCAN 0x1B

/* LVGL renders directly into g_shadow (we own it, freed in app_exit).
 * push_shadow_to_screen() blits it onto the PEG screen. */
static uint8_t* g_shadow;
static uint32_t g_shadow_bytes;

static volatile uint32_t g_draw_calls;
static volatile uint32_t g_msg_calls;
static volatile uint16_t g_last_msg_type;
static volatile uint16_t g_last_msg_subtype;
static volatile int32_t g_last_msg_data;

/* Ring buffer of recent wTypes so we can see the sequence of dispatches
 * regardless of frequency. */
#define WTYPE_HISTORY_LEN 12
static volatile uint16_t g_wtype_history[WTYPE_HISTORY_LEN];
static volatile uint8_t g_wtype_head;

/* Count of each wType we've received (indexed by type; only first 64 kept
 * to keep the array small). Higher types get bucketed into slot 63. */
static volatile uint32_t g_wtype_counts[64];

/* ---- PEG tick-rate probe (see PEG_SetTimer call in main) ---- */
#define PROBE_TIMER_ID 0x50EC
#define PROBE_TIMER_TICKS 50 /* per docs, ONE_SECOND is typically 50 */
static volatile uint32_t g_probe_fires;
static volatile uint32_t g_probe_first_ms;
static volatile uint32_t g_probe_last_ms;
static volatile uint32_t g_probe_last_delta_ms;

/* Captured base Message handler for delegation */
static msg_fn_t g_base_msg;

/* Forward decls (definitions live at the bottom of the file) */
uint32_t tstmr_get_millis(void);
void my_sleep(uint32_t millis);

/* LVGL indev shared state (produced by our_Message, consumed by read_cb) */
static lv_indev_t* g_indev_pointer;
static lv_indev_t* g_indev_keypad;
static volatile int16_t g_touch_x;
static volatile int16_t g_touch_y;
static volatile lv_indev_state_t g_touch_state = LV_INDEV_STATE_REL;
static volatile uint32_t g_key_pending;

/* Last-input snapshots for the debug label */
static volatile int16_t  g_last_touch_x = -1;
static volatile int16_t  g_last_touch_y = -1;
static volatile uint8_t  g_last_touch_state;   /* 0=up, 1=down */

/* Snapshot of the last PM_KEY* PegMessage for the debug label. Param is
 * the actual key code; ExtParams[0] holds Brain's state/modifier bits. */
static volatile int32_t  g_last_key_seen = 0;   /* 0 = never seen */
static volatile uint16_t g_last_key_type;       /* PM_KEY / PM_KEY_DOWN */
static volatile uint16_t g_last_key_param;
static volatile int32_t  g_last_key_ext0;
static volatile int32_t  g_last_key_ext1;
static volatile int16_t  g_last_key_ptx;
static volatile int16_t  g_last_key_pty;
static volatile lv_indev_state_t g_key_state = LV_INDEV_STATE_REL;

/* Brain key code (in PegMessage.Param) → LVGL key constant. */
static uint32_t peg_key_to_lvgl(int32_t k) {
    switch (k) {
        case 0x0D:  return LV_KEY_ENTER;
        case 0x1B:  return LV_KEY_ESC;
        case 0x08:  return LV_KEY_BACKSPACE;
        case 0x20:  return ' ';
        case 0x25E: return LV_KEY_LEFT;
        case 0x25F: return LV_KEY_DOWN;
        case 0x260: return LV_KEY_UP;
        case 0x261: return LV_KEY_RIGHT;
        case 0x265: return 0;   /* SHIFT (sticky — sets modifier bit 0x40 in
                                 * ExtParams[0] on the next real keypress) */
        default:    return (uint32_t)k;
    }
}

static void indev_pointer_read(lv_indev_t* d, lv_indev_data_t* out) {
    (void)d;
    out->point.x = g_touch_x;
    out->point.y = g_touch_y;
    out->state = g_touch_state;
}
static void indev_keypad_read(lv_indev_t* d, lv_indev_data_t* out) {
    (void)d;
    out->key = g_key_pending;
    out->state = g_key_state;
    if (g_key_state == LV_INDEV_STATE_PR)
        g_key_state = LV_INDEV_STATE_REL;
}

/* Tear down PEG + LVGL, then jump back to the launcher.
 * sh_ExitApp never returns on success.
 *
 * Order matters:
 *   1. KillTimer — stop any future PM_TIMER dispatch to a soon-to-be-freed
 *      window.
 *   2. PEG_Destroy — posts PM_DESTROY; PresMgr unlinks us from its window
 *      tree and runs the dtor (which free()s the window). Async, but PEG
 *      typically processes the queue before yielding to the shell.
 *   3. lv_deinit — releases every LVGL-owned allocation (display, indevs,
 *      timers, screen widgets). Uses our free() → sys_free trampoline.
 *   4. free(shadow) — LVGL v9's lv_display_set_buffers doesn't take
 *      ownership, so the caller (us) owns it.
 *   5. sh_ExitApp — launcher takes over.
 */
static void app_exit(void) {
    if (g_window) {
        PEG_KillTimer(g_window, PROBE_TIMER_ID);
        PEG_Destroy(g_window, g_window);
        g_window = NULL;
    }
    lv_deinit();
    if (g_shadow) {
        free(g_shadow);
        g_shadow = NULL;
    }
    if (g_exec_path) sh_ExitApp(g_exec_path); // without this, screen goes mysterious green'ish window 
    /* Should not reach here. Fall through returns to caller; PEG scheduler
     * takes over again which is at least survivable. */
}

/* Push our LVGL shadow through PEG's proper draw pipeline into PEG's
 * internal shadow, then EndDraw's MemoryToScreen flushes to physical FB.
 * Doing this synchronously means we don't depend on PEG's Draw dispatch
 * (which never fires on us — the dirty walker skips windows that aren't
 * registered as top-level focus targets via STATUS bit 0x40). */
static void push_shadow_to_screen(void) {
    if (!g_shadow || !g_window)
        return;

    /* mReal is embedded in every PegThing at +0x0C */
    const struct PegRect* pRect = (const struct PegRect*)((char*)g_window + 0x0C);
    PEG_WinBeginDraw(g_window, pRect);

    g_lvgl_pegbmp.bpp = 0x10;
    g_lvgl_pegbmp.flags = 0x00;
    g_lvgl_pegbmp.wWidth = (uint16_t)g_fb_w;
    g_lvgl_pegbmp.wHeight = (uint16_t)g_fb_h;
    g_lvgl_pegbmp.reserved = 0;
    g_lvgl_pegbmp.refcount = 1;
    g_lvgl_pegbmp.pData = g_shadow;

    void* scr = PEG_GetScreen();
    if (scr) {
        void** vt = *(void***)scr;
        ((scr_bitmap_fn)vt[SCR_VF_BITMAP])(scr, 0, &g_lvgl_pegbmp);
        ((scr_enddraw_fn)vt[SCR_VF_ENDDRAW])(scr);
    }
}

/* Debug label refreshed by a periodic LVGL timer */
static lv_obj_t* g_dbg_label;
static void refresh_cb(lv_timer_t* t) {
    (void)t;
    if (g_dbg_label) {
        char buf[256];
        int n = 0;
        n += lv_snprintf(buf + n, sizeof(buf) - n,
                         "drw=%u msg=%u\n",
                         (unsigned)g_draw_calls, (unsigned)g_msg_calls);
        n += lv_snprintf(buf + n, sizeof(buf) - n,
                         "hist:");
        for (int i = 0; i < WTYPE_HISTORY_LEN && n < (int)sizeof(buf) - 8; i++) {
            int idx = (g_wtype_head + i) % WTYPE_HISTORY_LEN;
            n += lv_snprintf(buf + n, sizeof(buf) - n, " %u",
                             (unsigned)g_wtype_history[idx]);
        }
        n += lv_snprintf(buf + n, sizeof(buf) - n, "\n");
        static const uint8_t interesting[] = {4, 6, 8, 9, 10, 11, 17, 24, 25, 28, 30, 32, 33};
        for (unsigned i = 0; i < sizeof(interesting) && n < (int)sizeof(buf) - 16; i++) {
            uint8_t k = interesting[i];
            if (g_wtype_counts[k]) {
                n += lv_snprintf(buf + n, sizeof(buf) - n, " t%u=%u",
                                 k, (unsigned)g_wtype_counts[k]);
            }
        }
        /* Last touch — coordinates + state (DN/UP) */
        if (g_last_touch_x >= 0) {
            n += lv_snprintf(buf + n, sizeof(buf) - n,
                             "\ntouch: (%d,%d) %s",
                             (int)g_last_touch_x, (int)g_last_touch_y,
                             g_last_touch_state ? "DN" : "UP");
        }
        /* Last key event: param = key code, ext0 flags (0x800 press,
         * 0xC00 held, 0x40 SHIFT-sticky active). */
        if (g_last_key_seen) {
            n += lv_snprintf(buf + n, sizeof(buf) - n,
                             "\nkey t=%u param=0x%X ext0=0x%X ext1=0x%X"
                             "\n     pt=(%d,%d)",
                             (unsigned)g_last_key_type,
                             (unsigned)g_last_key_param,
                             (unsigned)g_last_key_ext0,
                             (unsigned)g_last_key_ext1,
                             (int)g_last_key_ptx, (int)g_last_key_pty);
        }
        /* Tick-rate probe: show fires, last delta, and cumulative average.
         * SetTimer requested PROBE_TIMER_TICKS ticks per fire; delta_ms
         * lets us back-calculate ms per tick. */
        if (g_probe_fires > 0) {
            uint32_t avg = (g_probe_last_ms - g_probe_first_ms) /
                           (g_probe_fires > 1 ? g_probe_fires - 1 : 1);
            n += lv_snprintf(buf + n, sizeof(buf) - n,
                             "\ntimer fires=%u last=%ums avg=%ums (tick~%u.%02ums)",
                             (unsigned)g_probe_fires,
                             (unsigned)g_probe_last_delta_ms,
                             (unsigned)avg,
                             (unsigned)(avg / PROBE_TIMER_TICKS),
                             (unsigned)((avg * 100u / PROBE_TIMER_TICKS) % 100u));
        }
        lv_label_set_text(g_dbg_label, buf);
    }
    push_shadow_to_screen();
}

/* Draw override (vtable slot 12). Not currently dispatched by PEG in our
 * setup — kept as a reference implementation of the proper PEG draw
 * pipeline in case a future subclass configuration triggers it. */
static void our_Draw(void* self, const struct PegRect* invalid) {
    g_draw_calls++;

    /* Probe (60x60 top-left, colour cycles) */
    if (g_shadow) {
        uint16_t* px = (uint16_t*)g_shadow;
        uint16_t c = (uint16_t)((g_draw_calls * 0x1111) | 0x0800);
        if (g_draw_calls & 1)
            c ^= 0xF800;
        for (int y = 0; y < 60; y++) {
            uint16_t* row = px + (uint32_t)y * g_fb_w;
            for (int x = 0; x < 60; x++)
                row[x] = c;
        }
    }

    lv_timer_handler();

    /* Proper Draw pattern per PEG docs:
     *   BeginDraw(invalidRect) → base Draw / custom → EndDraw() */
    PEG_WinBeginDraw(self, invalid);
    PEG_WinBaseDraw(self, invalid);

    /* Custom overlay: blit our LVGL shadow */
    if (g_shadow) {
        g_lvgl_pegbmp.bpp = 0x10;
        g_lvgl_pegbmp.flags = 0x00;
        g_lvgl_pegbmp.wWidth = (uint16_t)g_fb_w;
        g_lvgl_pegbmp.wHeight = (uint16_t)g_fb_h;
        g_lvgl_pegbmp.reserved = 0;
        g_lvgl_pegbmp.refcount = 1;
        g_lvgl_pegbmp.pData = g_shadow;

        void* scr = PEG_GetScreen();
        if (scr) {
            void** scr_vt = *(void***)scr;
            ((scr_bitmap_fn)scr_vt[SCR_VF_BITMAP])(scr, 0, &g_lvgl_pegbmp);
        }
    }

    /* Matching EndDraw — Screen()->EndDraw() per PegThing::EndDraw inline */
    {
        void* scr = PEG_GetScreen();
        if (scr) {
            void** scr_vt = *(void***)scr;
            ((scr_enddraw_fn)scr_vt[SCR_VF_ENDDRAW])(scr);
        }
    }
    /* NO self-Invalidate — nesting BeginDraw/EndDraw inside our_Draw is
     * balanced. Continuous updates need a separate Invalidate path. */
}

/* ===== Message override — dispatch input events into LVGL indev ========= */
static int32_t our_Message(void* self, const struct PegMessage* msg) {
    unsigned short t = msg->Type;
    unsigned short param = msg->Param;

    g_msg_calls++;
    g_last_msg_type = t;
    g_last_msg_subtype = param;
    g_last_msg_data = msg->ExtParams[0];

    /* Ring buffer of wTypes we've seen */
    g_wtype_history[g_wtype_head] = t;
    g_wtype_head = (uint8_t)((g_wtype_head + 1) % WTYPE_HISTORY_LEN);
    g_wtype_counts[t < 64 ? t : 63]++;

    /* Small Message probe (top-right corner) — just for visual confirmation
     * that Message dispatch is reaching us. Written into shadow only;
     * push_shadow_to_screen below flushes it. */
    if (g_shadow) {
        uint16_t* sh = (uint16_t*)g_shadow;
        uint16_t c = (uint16_t)((g_msg_calls * 0x2108) | 0x0400);
        int32_t x0 = g_fb_w - 60;
        for (int y = 0; y < 60; y++) {
            uint16_t* row = sh + (uint32_t)y * g_fb_w + x0;
            for (int x = 0; x < 60; x++)
                row[x] = c;
        }
        int32_t bar_len = (t > 63 ? 63 : t) * 4;
        for (int y = 60; y < 70; y++) {
            uint16_t* row = sh + (uint32_t)y * g_fb_w + x0;
            for (int x = 0; x < bar_len; x++)
                row[x] = 0xF800;
            for (int x = bar_len; x < 60; x++)
                row[x] = 0x0000;
        }
    }

    switch (t) {
        /* Pointer / touch — position lives in the union's Point field */
        case PM_LBUTTONDOWN:
            g_touch_x = (int16_t)msg->Point.x;
            g_touch_y = (int16_t)msg->Point.y;
            g_touch_state = LV_INDEV_STATE_PR;
            g_last_touch_x = g_touch_x;
            g_last_touch_y = g_touch_y;
            g_last_touch_state = 1;
            break;
        case PM_LBUTTONUP:
            g_touch_x = (int16_t)msg->Point.x;
            g_touch_y = (int16_t)msg->Point.y;
            g_touch_state = LV_INDEV_STATE_REL;
            g_last_touch_x = g_touch_x;
            g_last_touch_y = g_touch_y;
            g_last_touch_state = 0;
            break;

        /* Key input — Brain layout:
         *   Param       = raw key code (e.g. 0x265 = SHIFT, 0x1B = ESC, ASCII)
         *   ExtParams[0]= state/modifier flags (0x800 = press, 0xC00 = held,
         *                 bit 0x40 = SHIFT-sticky is active)
         *   ExtParams[1]= (unused) */
        case PM_KEY:
        case PM_KEY_DOWN: {
            int32_t code = (int32_t)param;
            g_last_key_seen  = 1;
            g_last_key_type  = t;
            g_last_key_param = param;
            g_last_key_ext0  = msg->ExtParams[0];
            g_last_key_ext1  = msg->ExtParams[1];
            g_last_key_ptx   = (int16_t)msg->Point.x;
            g_last_key_pty   = (int16_t)msg->Point.y;
            if (code == EXIT_KEY_SCAN) {
                app_exit();      /* never returns on success */
                return 0;
            }
            uint32_t k = peg_key_to_lvgl(code);
            if (k) {
                g_key_pending = k;
                g_key_state = LV_INDEV_STATE_PR;
            }
            break;
        }
        case PM_KEY_UP:
            g_key_state = LV_INDEV_STATE_REL;
            break;

        case PM_TIMER: {
            uint32_t now = tstmr_get_millis();
            if (param == PROBE_TIMER_ID) {
                if (g_probe_fires == 0) {
                    g_probe_first_ms = now;
                    g_probe_last_delta_ms = 0;
                } else {
                    g_probe_last_delta_ms = now - g_probe_last_ms;
                }
                g_probe_last_ms = now;
                g_probe_fires++;
            }
            break;
        }
    }

    /* Tick LVGL on every message (input drives widget state changes),
     * then push the result to the physical screen through PEG's pipeline. */
    lv_timer_handler();
    push_shadow_to_screen();

    /* Delegate to base so PEG's own bookkeeping runs. */
    return g_base_msg(self, msg);
}

/* flush_cb: g_shadow IS the LVGL frame buffer, so nothing to copy.
 * push_shadow_to_screen() takes care of moving it to the panel. */
static void shadow_flush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_buf) {
    (void)area;
    (void)px_buf;
    lv_display_flush_ready(disp);
}

/* Private vtable — what a C++ compiler would auto-generate for a subclass.
 * PEG's PegDecoratedWindow uses a shared static vtable in ROM (0x600DAF88),
 * so we copy it here, patch our overridden slots, and swap our window's
 * vptr to this array. Heap allocation would work equally well; static is
 * chosen for simpler debugging (fixed address). */
static void* g_static_vt[80] __attribute__((aligned(16)));

__attribute__((section(".text.init"))) void main(app_ctx* ctx) {
    /* Save exec_path so app_exit() can pass it to sh_ExitApp later */
    if (ctx) g_exec_path = ctx->exec_path;

    /* --- Panel geometry --- */
    if (ctx && ctx->pPresent) {
        short rect[4] = {0};
        PEG_ScreenRect(ctx->pPresent, rect);
        int32_t w = rect[2] - rect[0] + 1;
        int32_t h = rect[3] - rect[1] + 1;
        if (w >= 64 && w <= 4096 && h >= 64 && h <= 4096) {
            g_fb_w = w;
            g_fb_h = h;
        }
    }

    /* --- Shadow buffer LVGL renders into --- */
    g_shadow_bytes = (uint32_t)g_fb_w * (uint32_t)g_fb_h * 2u;
    g_shadow = (uint8_t*)lv_malloc(g_shadow_bytes);

    /* --- LVGL bring-up (rendering into shadow buffer) --- */
    lv_init();
    lv_tick_set_cb(tstmr_get_millis);
    lv_display_t* display = lv_display_create(g_fb_w, g_fb_h);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, g_shadow, NULL, g_shadow_bytes,
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, shadow_flush);

    /* ---- LVGL widget setup ---- */
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x102040), 0); /* dark blue-grey */
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t* label = lv_label_create(scr);
    lv_label_set_text_fmt(label,
                          "LVGL via PEG\n%ldx%ld RGB565", (long)g_fb_w, (long)g_fb_h);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    /* A colourful rectangle so we see a solid block regardless of font */
    lv_obj_t* box = lv_obj_create(scr);
    lv_obj_set_size(box, 200, 100);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x00C000), 0);
    lv_obj_set_style_border_width(box, 4, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0xFF8000), 0);

    /* Debug label at bottom: msg counts, wType histogram, last touch/key,
     * PEG-timer probe stats. */
    lv_obj_t* dbg = lv_label_create(scr);
    lv_obj_set_style_text_color(dbg, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_bg_color(dbg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(dbg, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(dbg, 4, 0);
    lv_obj_align(dbg, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_label_set_text(dbg, "waiting...");
    g_dbg_label = dbg;

    /* Setup indev */
    g_indev_pointer = lv_indev_create();
    lv_indev_set_type(g_indev_pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_indev_pointer, indev_pointer_read);

    g_indev_keypad = lv_indev_create();
    lv_indev_set_type(g_indev_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(g_indev_keypad, indev_keypad_read);

    /* Render into shadow a few times to complete initial paint */
    for (int i = 0; i < 5; i++)
        lv_timer_handler();

    /* Repeating LVGL timer: refresh debug label and push shadow → panel
     * so widgets animate even when no input arrives. */
    lv_timer_create(refresh_cb, 200, NULL);

    /* --- Register as a real PEG window --- */
    if (ctx && ctx->pPresent) {
        g_window = lv_malloc(0x108);
        if (g_window) {
            memset(g_window, 0, 0x108);

            /* Construct + place. AdjustClient recomputes mClient from
             * mReal + frame thickness; harmless even though we bypass
             * PEG's client-area drawing. */
            PEG_DecWindow_ctor(g_window, AF_ENABLED | FF_THIN);
            PEG_RectSet((short*)((char*)g_window + 0x0C),
                        0, 0, (short)(g_fb_w - 1), (short)(g_fb_h - 1));
            PEG_AdjustClient(g_window);

            /* Manual subclassing: copy the shared PegDecoratedWindow vtable,
             * patch our overridden slots, and point our instance at it.
             * (What a C++ compiler would do automatically for us.) */
            void** base_vt = *(void***)g_window;
            for (int i = 0; i < 80; i++)
                g_static_vt[i] = base_vt[i];
            g_base_msg = (msg_fn_t)base_vt[VFUNC_MESSAGE];
            g_static_vt[VFUNC_DRAW] = (void*)our_Draw;
            g_static_vt[VFUNC_MESSAGE] = (void*)our_Message;
            *(void**)g_window = g_static_vt;

            /* PegPresentationManager::Add(window, show=TRUE) */
            typedef void (*add_fn)(void*, void*, int);
            void** pm_vt = *(void***)ctx->pPresent;
            ((add_fn)pm_vt[3])(ctx->pPresent, g_window, 1);

            /* Route keyboard input to us without triggering the focus-event
             * chain (bit 0x40 pathway) which needs additional setup. */
            PEG_SetCurrent(ctx->pPresent, g_window);

            /* Kick off the PEG-tick probe: one PM_TIMER per PROBE_TIMER_TICKS
             * ticks, repeated. Handled in our_Message case PM_TIMER. */
            PEG_SetTimer(g_window, PROBE_TIMER_ID,
                         PROBE_TIMER_TICKS, PROBE_TIMER_TICKS);
        }
    }

    /* Return — cooperative scheduler, main-loop starves everything. */
    return;
}

/* ===== LVGL alloc hooks + libc stubs ================================== */
void* lv_malloc_core(size_t size) {
    return malloc(size);
}
void* lv_realloc_core(void* p, size_t size) {
    return realloc(p, size);
}
void lv_free_core(void* p) {
    free(p);
}
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_result_t lv_mem_test_core(void) {
    return LV_RESULT_OK;
}
void lv_mem_monitor_core(lv_mem_monitor_t* m) {
    (void)m;
}

uint32_t tstmr_get_millis(void) {
    uint32_t lo = *(volatile uint32_t*)0x410A3C00;
    uint32_t hi = *(volatile uint32_t*)0x410A3C04;
    return (uint32_t)(((((uint64_t)hi) << 32) | lo) / 1000);
}
void my_sleep(uint32_t millis) {
    uint32_t start = tstmr_get_millis();
    while (tstmr_get_millis() - start < millis) {
    }
}
