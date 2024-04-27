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

const UWORD depth = 4;
const UWORD screenWidth = 320;
const UWORD screenByteWidth = screenWidth / 8;
const UWORD screenHeight = 256;
ULONG screenSize = (screenWidth/8) * screenHeight * (depth+1); // One extra depth for carry
UBYTE *image;

__attribute__((always_inline)) inline void WaitBlt() {
	UWORD tst=*(volatile UWORD*)&custom->dmaconr; //for compatiblity a1000
	(void)tst;
	while (*(volatile UWORD*)&custom->dmaconr&(1<<14)) {} //blitter busy wait
}


APTR GetEffectBitplanes() {
    image = AllocMem(screenSize, MEMF_CHIP | MEMF_CLEAR);
    return image;
}

void FreeMemory() {
    FreeMem(image, screenSize);
}

const UWORD bobByteWidth = 6;
const UWORD bobHeight = 32;
const UWORD bobDepth = 4;

INCBIN_CHIP(bob, "bob.bpl")

void BlitCircle(UWORD x, UWORD y) {
    UWORD shift = x & 0x0f;
    UBYTE *dst = ((UBYTE*)image) + y * screenByteWidth * depth + (x >> 4);
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
void FadeOut() {
    WaitBlt();
    APTR carry = image + depth * screenByteWidth * screenHeight;
    WORD bltSize = (fadeOutHeight << HSIZEBITS) | screenByteWidth/2;
    WORD imageMod = (depth-1) * screenByteWidth;

    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltcon1 = 0;
    // bit 0
    custom->bltcon0 = 0x0f | SRCA | DEST;
    custom->bltapt = image;
    custom->bltamod = imageMod;    
    custom->bltdpt = image;
    custom->bltdmod = imageMod;
    custom->bltsize = bltSize;
    WaitBlt();
    // carry 0
    custom->bltapt = image;
    custom->bltamod = imageMod;
    custom->bltdpt = carry;
    custom->bltdmod = 0;
    custom->bltsize = bltSize;
    WaitBlt();
    // bit 1
    custom->bltcon0 = 0xc3 | SRCA | SRCB | DEST;
    custom->bltapt = image + screenByteWidth;
    custom->bltamod = imageMod;
    custom->bltbpt = carry;
    custom->bltbmod = 0;
    custom->bltdpt = image + screenByteWidth;
    custom->bltdmod = imageMod;
    custom->bltsize = bltSize;
    WaitBlt();
    // carry 1
    custom->bltcon0 = 0xcf | SRCA | SRCB | DEST;
    custom->bltapt = image + screenByteWidth;
    custom->bltamod = imageMod;
    custom->bltbpt = carry;
    custom->bltbmod = 0;
    custom->bltdpt = carry;
    custom->bltdmod = 0;
    custom->bltsize = bltSize;
    WaitBlt();
    // bit 2
    custom->bltcon0 = 0xc3 | SRCA | SRCB | DEST;
    custom->bltapt = image + screenByteWidth*2;
    custom->bltamod = imageMod;
    custom->bltbpt = carry;
    custom->bltbmod = 0;
    custom->bltdpt = image + screenByteWidth*2;
    custom->bltdmod = imageMod;
    custom->bltsize = bltSize;
    WaitBlt();
    // carry 2
    custom->bltcon0 = 0xcf | SRCA | SRCB | DEST;
    custom->bltapt = image + screenByteWidth*2;
    custom->bltamod = imageMod;
    custom->bltbpt = carry;
    custom->bltbmod = 0;
    custom->bltdpt = carry;
    custom->bltdmod = 0;
    custom->bltsize = bltSize;
    WaitBlt();
    // bit 3
    custom->bltcon0 = 0xc3 | SRCA | SRCB | DEST;
    custom->bltapt = image + screenByteWidth*3;
    custom->bltamod = imageMod;
    custom->bltbpt = carry;
    custom->bltbmod = 0;
    custom->bltdpt = image + screenByteWidth*3;
    custom->bltdmod = imageMod;
    custom->bltsize = bltSize;
    WaitBlit();
    // What if we use the CPU?
 
}

static const UWORD sinus40[] = {
	20,22,24,26,28,30,31,33,
	34,36,37,38,39,39,40,40,
	40,40,39,39,38,37,36,35,
	34,32,30,29,27,25,23,21,
	19,17,15,13,11,10,8,6,
	5,4,3,2,1,1,0,0,
	0,0,1,1,2,3,4,6,
	7,9,10,12,14,16,18,20,
};
UWORD frame = 0;

void CalcEffect() {

    frame++;

    UWORD pos = frame >> 3;

    UWORD sin = sinus40[pos&0x3f];
    UWORD cos = sinus40[(pos+16)&0x3f];
    BlitCircle((UWORD)50+sin, (UWORD)10+cos);
    FadeOut();
}