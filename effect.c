#include "support/gcc8_c_support.h"
#include <exec/types.h>
#include <proto/exec.h>
#include <graphics/gfxmacros.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>
#include <proto/graphics.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include "effect.h"
#include "hardware.h"
#include "screen.h"

const UWORD depth = 4;
const UWORD screenWidth = 320;
const UWORD screenByteWidth = screenWidth / 8;
const UWORD screenHeight = 256;
ULONG screenSize = (screenWidth/8) * screenHeight * (depth); // One extra depth for carry
UBYTE *buf0;
UBYTE *buf1;
UBYTE *buf;
UBYTE *carry;

__attribute__((always_inline)) inline void WaitBlt() {
	UWORD tst=*(volatile UWORD*)&custom->dmaconr; //for compatiblity a1000
	(void)tst;
	while (*(volatile UWORD*)&custom->dmaconr&(1<<14)) {} //blitter busy wait
}

void InitEffect() {
    buf0 = AllocMem(screenSize, MEMF_CHIP | MEMF_CLEAR);
    buf1 = AllocMem(screenSize, MEMF_CHIP);
    buf = buf0;
    carry = AllocMem(screenByteWidth * screenHeight, MEMF_CHIP);
    custom->dmacon = DMAF_SETCLR | DMAF_BLITHOG;
   	SetupScreen(buf0);
}


void FreeEffect() {
    FreeMem(buf0, screenSize);
    FreeMem(buf1, screenSize);
    FreeMem(carry, screenByteWidth * screenHeight);
}

const UWORD bobByteWidth = 6;
const UWORD bobHeight = 32;
const UWORD bobDepth = 4;

INCBIN_CHIP(bob, "bob.bpl")

void BlitCircle(UBYTE *buf, UWORD x, UWORD y) {
    UWORD shift = x & 0x0f;
    UBYTE *dst = ((UBYTE*)buf) + y * screenByteWidth * depth + ((x >> 3)& 0xffe);
    WaitBlt();
    custom->bltcon0 = 0xfc | SRCA | SRCB | DEST | ( shift << ASHIFTSHIFT);
    custom->bltcon1 = 0;
    custom->bltapt = (APTR)bob;
    custom->bltamod = 0;
    custom->bltbpt = dst;
    custom->bltbmod = screenByteWidth - bobByteWidth;
    custom->bltdpt = dst;
    custom->bltdmod = screenByteWidth - bobByteWidth;
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltsize = (((bobHeight * depth)<< HSIZEBITS) ) | ((bobByteWidth / 2));    
}

// DST -> A, C -> B, DST ->D
// Bit 0
// A|bC
// 0|11
// 1|00
// Bit 1,2,3(no Carry)
// AC|bC 
// 00|11
// 01|01
// 10|00
// 11|11

const UWORD fadeOutHeight = 64;
void FadeOut(UBYTE *buf) {
    WaitBlt();
    WORD bltSize = (fadeOutHeight << HSIZEBITS) | screenByteWidth/2;
    WORD imageMod = (depth-1) * screenByteWidth;

    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltcon1 = 0;
    // bit 0
    custom->bltcon0 = 0x0f | SRCA | DEST;
    custom->bltapt = buf;
    custom->bltamod = imageMod;    
    custom->bltdpt = buf;
    custom->bltdmod = imageMod;
    custom->bltsize = bltSize;
    WaitBlt();
    // carry 0
    custom->bltapt = buf;
    custom->bltamod = imageMod;
    custom->bltdpt = carry;
    custom->bltdmod = 0;
    custom->bltsize = bltSize;
    WaitBlt();
    // bit 1
    custom->bltcon0 = 0xc3 | SRCA | SRCB | DEST;
    custom->bltapt = buf + screenByteWidth;
    custom->bltamod = imageMod;
    custom->bltbpt = carry;
    custom->bltbmod = 0;
    custom->bltdpt = buf + screenByteWidth;
    custom->bltdmod = imageMod;
    custom->bltsize = bltSize;
    WaitBlt();
    // carry 1
    custom->bltcon0 = 0xcf | SRCA | SRCB | DEST;
    custom->bltapt = buf + screenByteWidth;
    custom->bltamod = imageMod;
    custom->bltbpt = carry;
    custom->bltbmod = 0;
    custom->bltdpt = carry;
    custom->bltdmod = 0;
    custom->bltsize = bltSize;
    WaitBlt();
    // bit 2
    custom->bltcon0 = 0xc3 | SRCA | SRCB | DEST;
    custom->bltapt = buf + screenByteWidth*2;
    custom->bltamod = imageMod;
    custom->bltbpt = carry;
    custom->bltbmod = 0;
    custom->bltdpt = buf + screenByteWidth*2;
    custom->bltdmod = imageMod;
    custom->bltsize = bltSize;
    WaitBlt();
    // carry 2
    custom->bltcon0 = 0xcf | SRCA | SRCB | DEST;
    custom->bltapt = buf + screenByteWidth*2;
    custom->bltamod = imageMod;
    custom->bltbpt = carry;
    custom->bltbmod = 0;
    custom->bltdpt = carry;
    custom->bltdmod = 0;
    custom->bltsize = bltSize;
    WaitBlt();
    // bit 3
    custom->bltcon0 = 0xc3 | SRCA | SRCB | DEST;
    custom->bltapt = buf + screenByteWidth*3;
    custom->bltamod = imageMod;
    custom->bltbpt = carry;
    custom->bltbmod = 0;
    custom->bltdpt = buf + screenByteWidth*3;
    custom->bltdmod = imageMod;
    custom->bltsize = bltSize;    
    // What if we use the CPU?
}

void PlaneCopy(UBYTE *from, UBYTE *to) {
    WaitBlit();
    custom->bltcon0 = 0xf0 | SRCA | DEST;
    custom->bltcon1 = 0;
    custom->bltapt = from;
    custom->bltamod = 0;
    custom->bltdpt = to;
    custom->bltdmod = 0;
    custom->bltsize = ((128 * depth) << HSIZEBITS) | screenByteWidth/2;
    
}

static const UWORD sinus32[] = {
    16, 18, 19, 21, 22, 24, 25, 26, 
    27, 28, 29, 30, 31, 31, 32, 32, 
    32, 32, 32, 31, 31, 30, 29, 28, 
    27, 26, 25, 24, 22, 21, 19, 18, 
    16, 14, 13, 11, 10, 8, 7, 6, 5, 
    4, 3, 2, 1, 1, 0, 0, 0, 
    0, 0, 1, 1, 2, 3, 4, 5, 
    6, 7, 8, 10, 11, 13, 14
};

static const UWORD sinus256[64] = {
    128, 141, 153, 165, 177, 188, 199, 209, 219, 227, 234, 241, 246, 250, 254, 255, 256, 255, 254, 250,
    246, 241, 234, 227, 219, 209, 199, 188, 177, 165, 153, 141, 128, 115, 103, 91, 79, 68, 57, 47,
    37, 29, 22, 15, 10, 6, 2, 1, 0, 1, 2, 6, 10, 15, 22, 29, 37, 47, 57, 68, 79, 91, 103, 115
};


UWORD frame = 0;

void CalcEffect() {
    UBYTE *frontBuf;
    if ( buf == buf0 ) {
        buf = buf1;
        frontBuf = buf0;
    }
    else {
        buf = buf0;
        frontBuf = buf1;
    }

    SetPlanes(frontBuf);
    PlaneCopy(frontBuf, buf);


    frame++;
    UWORD pos = frame;
    
    UWORD sin = sinus256[pos&0x3f];    
    UWORD cos = sinus32[(pos+16)&0x3f];    

    BlitCircle(buf, (UWORD)0+sin, (UWORD)0+cos);
    FadeOut(buf);
}