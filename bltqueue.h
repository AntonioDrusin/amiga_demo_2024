#pragma once
#include "support/gcc8_c_support.h"
#include <hardware/custom.h>
#include <exec/types.h>

extern UWORD* BltQueue_copperList;
extern WORD last_bltafwm;
extern WORD last_bltalwm;
extern WORD last_bltcon0;
extern WORD last_bltcon1;
extern WORD last_bltamod;
extern WORD last_bltbmod;
extern WORD last_bltcmod;
extern WORD last_bltdmod;


extern void BltQueue_begin();
extern void BltQueue_fire();
extern void BltQueue_cleanup();
extern void BltQueue_end();
extern void BltQueue_wait();

__attribute__((always_inline)) inline void BltQueue_bltafwm (WORD value) {
    if ( value != last_bltafwm) {
        *BltQueue_copperList++ = offsetof(struct Custom, bltafwm);
        *BltQueue_copperList++ = value;
        last_bltafwm = value;
    }
}

__attribute__((always_inline)) inline void BltQueue_bltalwm (WORD value) {
    if ( value != last_bltalwm) {
        *BltQueue_copperList++ = offsetof(struct Custom, bltalwm);
        *BltQueue_copperList++ = value;
        last_bltalwm = value;
    }
}

__attribute__((always_inline)) inline void BltQueue_bltcon0 (WORD value) {
    if ( value != last_bltcon0) {
        *BltQueue_copperList++ = offsetof(struct Custom, bltcon0);
        *BltQueue_copperList++ = value;
        last_bltcon0 = value;
    }
}

__attribute__((always_inline)) inline void BltQueue_bltcon1 (WORD value) {
    if ( value != last_bltcon1) {
        *BltQueue_copperList++ = offsetof(struct Custom, bltcon1);
        *BltQueue_copperList++ = value;
        last_bltcon1 = value;
    }
}

__attribute__((always_inline)) inline void BltQueue_bltamod (WORD value) {
    if ( value != last_bltamod) {
        *BltQueue_copperList++ = offsetof(struct Custom, bltamod);
        *BltQueue_copperList++ = value;
        last_bltamod = value;
    }
}

__attribute__((always_inline)) inline void BltQueue_bltbmod (WORD value) {
    if ( value != last_bltbmod) {
        *BltQueue_copperList++ = offsetof(struct Custom, bltbmod);
        *BltQueue_copperList++ = value;
        last_bltbmod = value;
    }
}

__attribute__((always_inline)) inline void BltQueue_bltcmod (WORD value) {
    if ( value != last_bltcmod) {
        *BltQueue_copperList++ = offsetof(struct Custom, bltcmod);
        *BltQueue_copperList++ = value;
        last_bltcmod = value;
    }
}

__attribute__((always_inline)) inline void BltQueue_bltdmod (WORD value) {
    if ( value != last_bltdmod) {
        *BltQueue_copperList++ = offsetof(struct Custom, bltdmod);
        *BltQueue_copperList++ = value;
        last_bltdmod = value;
    }
}

__attribute__((always_inline)) inline void BltQueue_bltadat (WORD value) {
    *BltQueue_copperList++ = offsetof(struct Custom, bltadat);
    *BltQueue_copperList++ = value;
}

__attribute__((always_inline)) inline void BltQueue_bltapt (APTR value) {
    UWORD* longPtr = (UWORD*)&value;
    *BltQueue_copperList++ = offsetof(struct Custom, bltapt);
    *BltQueue_copperList++ = longPtr[0];
    *BltQueue_copperList++ = offsetof(struct Custom, bltapt)+2;
    *BltQueue_copperList++ = longPtr[1];
}

__attribute__((always_inline)) inline void BltQueue_bltbpt (APTR value) {
    UWORD* longPtr = (UWORD*)&value;
    *BltQueue_copperList++ = offsetof(struct Custom, bltbpt);
    *BltQueue_copperList++ = longPtr[0];
    *BltQueue_copperList++ = offsetof(struct Custom, bltbpt)+2;
    *BltQueue_copperList++ = longPtr[1];
    // ULONG longPtr = (ULONG)value;
    // *BltQueue_copperList++ = offsetof(struct Custom, bltbpt);
    // *BltQueue_copperList++ = longPtr >> 16;
    // *BltQueue_copperList++ = offsetof(struct Custom, bltbpt)+2;
    // *BltQueue_copperList++ = longPtr && 0xffff;
}

__attribute__((always_inline)) inline void BltQueue_bltcpt (APTR value) {
    UWORD* longPtr = (UWORD*)&value;
    *BltQueue_copperList++ = offsetof(struct Custom, bltcpt);
    *BltQueue_copperList++ = longPtr[0];
    *BltQueue_copperList++ = offsetof(struct Custom, bltcpt)+2;
    *BltQueue_copperList++ = longPtr[1];
}

__attribute__((always_inline)) inline void BltQueue_bltdpt (APTR value) {
    UWORD* longPtr = (UWORD*)&value;
    *BltQueue_copperList++ = offsetof(struct Custom, bltdpt);
    *BltQueue_copperList++ = longPtr[0];
    *BltQueue_copperList++ = offsetof(struct Custom, bltdpt)+2;
    *BltQueue_copperList++ = longPtr[1];
}

__attribute__((always_inline)) inline void BltQueue_bltsize (WORD value) {
    *BltQueue_copperList++ = offsetof(struct Custom, bltsize);
    *BltQueue_copperList++ = value;
    // Update copper ptr 2
    ULONG address = (ULONG)(BltQueue_copperList + 2);
    UWORD* longPtr = (UWORD *)&address;
    //*BltQueue_copperList++ = offsetof(struct Custom, cop2lc);
    //*BltQueue_copperList++ = longPtr[0];
    *BltQueue_copperList++ = offsetof(struct Custom, cop2lc)+2;
    *BltQueue_copperList++ = longPtr[1];
    // wait for blt done
    *BltQueue_copperList++ = 0x1;
    *BltQueue_copperList++ = 0x0;
    // wait for blt done (Is there a bug in some older amigas?)
    *BltQueue_copperList++ = 0x1;
    *BltQueue_copperList++ = 0x0;    
}
