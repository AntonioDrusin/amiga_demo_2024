#include <proto/exec.h>
#include <exec/types.h>
#include <hardware/blit.h>
#include "bltqueue.h"
#include "hardware.h"


typedef struct QueueTools {
  UWORD emptyCopperList[2];
  UWORD copperSignal[1];  
} QueueTools;

static UWORD* blitCopperlist = NULL;
static UBYTE* blitCopperlistMemory = NULL;
static QueueTools *queueTools = NULL;
static UWORD* blitterSignal = NULL;
UWORD *BltQueue_copperList;
WORD last_bltalwm; // a value that i am pretty sure I am not setting
WORD last_bltafwm;
WORD last_bltcon0;
WORD last_bltcon1;
WORD last_bltamod;
WORD last_bltbmod;
WORD last_bltcmod;
WORD last_bltdmod;
const WORD blitCopperListSize = 30000; // in bytes. Worst case only half of this can be used

// Returns pointer to a valid and empty copperlist2
void BltQueue_init() {
    if (blitCopperlist == NULL) {
      blitCopperlistMemory = AllocMem(blitCopperListSize, MEMF_CHIP);
      UWORD halfSize = blitCopperListSize / 2;
      if ( (UWORD)((ULONG)blitCopperlistMemory & 0x0ffff) + halfSize < halfSize) {
        blitCopperlist = (UWORD*)((((ULONG)blitCopperlistMemory) & 0xffff0000) + 0x00010000);
      }
      else {
        blitCopperlist = (UWORD*)blitCopperlistMemory;
      }

      queueTools = AllocMem(sizeof(QueueTools), MEMF_CHIP);
      debug_register_copperlist(blitCopperlist, "copper2", blitCopperListSize/2, 0);
      debug_register_copperlist(queueTools->emptyCopperList, "copper_dummy", 16, 0);
    }
    
    BltQueue_copperList = queueTools->emptyCopperList;
    *BltQueue_copperList++ = 0xffff;
    *BltQueue_copperList++ = 0xfffe;
    custom->cop2lc = (ULONG)queueTools->emptyCopperList;
};

void BltQueue_cleanup() {
    if ( blitCopperlist != NULL) {
      FreeMem(blitCopperlistMemory, blitCopperListSize);
      FreeMem(queueTools, sizeof(QueueTools));
    }
}

void BltQueue_fire() {
    custom->cop2lc = (ULONG)blitCopperlist;
    custom->copjmp2 = 0x7fff;
}

void BltQueue_wait() {
    //KPrintF("%ld",((ULONG)BltQueue_copperList-(ULONG)blitCopperlist));
    volatile UWORD *copSig = queueTools->copperSignal;
    while (!*copSig);
}

void BltQueue_begin() {
  custom->cop2lc = (ULONG)queueTools->emptyCopperList;
  last_bltafwm = 0xaf01; // a value that i am pretty sure I am not setting
  last_bltafwm = 0xaf01;
  last_bltcon0 = 0xffff;
  last_bltcon1 = 0xffff;
  last_bltamod = 0x7fff;
  last_bltbmod = 0x7fff;
  last_bltcmod = 0x7fff;
  last_bltdmod = 0x7fff;
  BltQueue_copperList = blitCopperlist;
};

void BltQueue_end () {
    queueTools->copperSignal[0]= 0x0;
    BltQueue_bltcon0(0xf0 | DEST);
    BltQueue_bltcon1(0);
    BltQueue_bltdpt(queueTools->copperSignal);
    BltQueue_bltsize(1 << HSIZEBITS | 1);

    ULONG address = (ULONG)(BltQueue_copperList + 4);
    UWORD* longPtr = (UWORD *)&address;
    *BltQueue_copperList++ = offsetof(struct Custom, cop2lc);
    *BltQueue_copperList++ = longPtr[0];
    *BltQueue_copperList++ = offsetof(struct Custom, cop2lc)+2;
    *BltQueue_copperList++ = longPtr[1];
    
    *BltQueue_copperList++ = 0xffff;
    *BltQueue_copperList++ = 0xfffe;
}
