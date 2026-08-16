#ifndef PEG_CONST_H
#define PEG_CONST_H

/* PegThing frame-style values (from PEG API docs) */
#define FF_NONE 0
#define FF_THIN 1
#define FF_RAISED 2
#define FF_RECESSED 3
#define FF_THICK 4

#define AF_ENABLED 0x10

/* ===== PegMessage.Type constants (Brain-specific numbering) =============
 * Values come from watching wTypes arrive in our_Message on real hardware.
 * They do NOT match the generic PEG SDK numbering. Field usage per type:
 *   PM_KEY / PM_KEY_DOWN — Param = key code (0x265=SHIFT, 0x1B=ESC, ASCII,
 *                          arrows 0x25E..0x261). ExtParams[0] = KFLAG_* /
 *                          KMOD_* bits below. ExtParams[1] unused.
 *   PM_LBUTTONDOWN/UP    — Point.x/y = touch coordinates.
 *   PM_TIMER             — Param = timer Id passed to SetTimer. */
#define PM_KEY           9
#define PM_LBUTTONDOWN   10
#define PM_LBUTTONUP     11
#define PM_TIMER         32
#define PM_KEY_DOWN      33
#define PM_KEY_UP        34   /* tentative — needs confirmation */

/* Bits found in PegMessage.ExtParams[0] on key messages */
#define KMOD_SHIFT       0x40   /* SHIFT-sticky is engaged for this event */
#define KFLAG_PRESS      0x800  /* set on any key press */
#define KFLAG_HELD       0xC00  /* set while a key is held down (repeat) */

struct PegPoint {
    unsigned short x;
    unsigned short y;
};

struct PegRect {
    unsigned short Left;
    unsigned short Top;
    unsigned short Right;
    unsigned short Bottom;
};

struct PegMessage {
    void* pSource;
    void* pTarget;
    unsigned short Type;
    unsigned short Param;

    union {
        struct PegRect Rect;
        struct PegPoint Point;
        int ExtParams[2];
        void* pData;
        int UserLong[2];
        unsigned int UserULONG[2];
        short UserShoft[4];
        unsigned short UserUShoft[4];
        unsigned char UserUByte[8];
    };
    void* pNext;
};

#endif  // PEG_CONST_H