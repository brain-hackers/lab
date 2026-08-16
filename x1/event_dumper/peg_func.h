#ifndef PEG_FUNC_H
#define PEG_FUNC_H

#include "peg_const.h"

/* ===== Function-pointer typedefs =========================================
 * Since we can't run C++ static init (no ELF loader), method prototypes
 * from the PEG SDK are re-declared here as function pointers, addressed
 * directly at the fixed kernel export slots. */

typedef void* (*decwin_ctor_proc)   (void* self, unsigned short style);
typedef void  (*pegrect_set_proc)   (short* rect, short x1, short y1,
                                     short x2, short y2);
typedef void  (*screen_rect_proc)   (void* pm, short out_rect[4]);
typedef void  (*adjust_client_proc) (void* self);
typedef void  (*win_begin_draw_proc)(void* self, const struct PegRect* rect);
typedef void  (*win_base_draw_proc) (void* self, const struct PegRect* rect);
typedef void* (*get_screen_proc)    (void);
typedef void  (*set_current_proc)   (void* pm, void* target);
typedef void  (*pt_settimer_proc)   (void* pWho, unsigned int Id,
                                     unsigned int Count, unsigned int Reset);
typedef void  (*pt_killtimer_proc)  (void* pWho, unsigned int Id);
typedef void  (*pt_destroy_proc)    (void* pSource, void* pTarget);
typedef void  (*sh_exitapp_proc)    (char* path);

/* PegScreen vtable slots */
typedef struct __attribute__((packed)) {
    unsigned char  bpp;
    unsigned char  flags;
    unsigned short wWidth;
    unsigned short wHeight;
    unsigned short reserved;
    unsigned int   refcount;
    unsigned char* pData;
} PegBitmap;
typedef void (*scr_bitmap_fn) (void* scr, int packed_xy, PegBitmap* bmp);
typedef void (*scr_enddraw_fn)(void* scr);

/* Signature of every PegThing::Message override (vfunc[19]) */
typedef int (*msg_fn_t)(void* self, const struct PegMessage* msg);

/* ===== Kernel export addresses (x1ram, verified) ========================
 * Most PEG_* addresses in the 0x60004000-0x60006FFF range are jump-table
 * trampolines (`ldr pc,[pc,#-4]; .word ADDR`); a few point straight at
 * the implementation. sh_ExitApp lives in Brain's shell area, not PEG. */

#define PEG_DecWindow_ctor  ((decwin_ctor_proc)   0x6000486c)  /* ord 0x09 */
#define PEG_RectSet         ((pegrect_set_proc)   0x60004130)  /* ord 0x23 */
#define PEG_ScreenRect      ((screen_rect_proc)   0x60005358)  /* ord 0x0B */
#define PEG_AdjustClient    ((adjust_client_proc) 0x60004850)  /* ord 0x07 */
#define PEG_WinBeginDraw    ((win_begin_draw_proc)0x60004590)  /* → 6010D050 → screen.vfunc[3] */
#define PEG_WinBaseDraw     ((win_base_draw_proc) 0x6000602C)  /* → 600478EC = PegWindow::Draw base */
#define PEG_GetScreen       ((get_screen_proc)    0x60004144)  /* → 600B5520 */
#define PEG_SetCurrent      ((set_current_proc)   0x600313C8)  /* PresMgr, routes key input */
#define PEG_SetTimer        ((pt_settimer_proc)   0x600470A0)  /* PegThing wrapper — Count/Reset in PEG ticks */
#define PEG_KillTimer       ((pt_killtimer_proc)  0x600470D0)  /* PegThing wrapper */
#define PEG_Destroy         ((pt_destroy_proc)    0x6002A3E4)  /* posts PM_DESTROY to pTarget */

/* Brain shell function — hands control back to the launcher.
 * NEVER RETURNS; call after cleaning up owned resources. */
#define sh_ExitApp          ((sh_exitapp_proc)    0x840E7A48)

/* PegScreen vtable slots we call directly */
#define SCR_VF_BITMAP  4    /* Bitmap blit — screen->vfunc[4](packed_xy, bmp) */
#define SCR_VF_ENDDRAW 16   /* EndDraw     — flushes MemoryToScreen when nesting=0 */

/* PegThing vtable slots we override */
#define VFUNC_DRAW    12
#define VFUNC_MESSAGE 19

#endif  /* PEG_FUNC_H */
