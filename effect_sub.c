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
#include "effect_sub.h"
#include "hardware.h"
#include "screen.h"

const UWORD depth = 4;
const UWORD screenWidth = 320;
const UWORD screenByteWidth = screenWidth / 8;
const UWORD screenHeight = 256;
const UWORD vertSpeed = 4;

// Naive 15 color can do 46 lines.
const UWORD fadeOutHeight = 195; 
ULONG screenSize = (screenWidth/8) * screenHeight * (depth); // One extra depth for carry
UBYTE *buf0;
UBYTE *buf1;
UBYTE *buf;

__attribute__((always_inline)) inline void WaitBlt() {
	UWORD tst=*(volatile UWORD*)&custom->dmaconr; //for compatiblity a1000
	(void)tst;
	while (*(volatile UWORD*)&custom->dmaconr&(1<<14)) {} //blitter busy wait
}

ULONG v = 12323;
ULONG u = 3321;

static ULONG random() {
    v = 36969*(v & 65535) + (v >> 16);
    u = 18000*(u & 65535) + (u >> 16);
    return (v << 16) + (u & 65535);
}


const UWORD bobByteWidth = 6;
const UWORD bobHeight = 32;
const UWORD bobDepth = 4;

INCBIN_CHIP(bob, "bob.bpl")

static void PrepBlitCircle() {
    WaitBlt();
    custom->bltamod = bobByteWidth;
    custom->bltbmod = bobByteWidth;
    custom->bltcmod = screenByteWidth - bobByteWidth;
    custom->bltdmod = screenByteWidth - bobByteWidth;
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;

}
static void BlitCircle(UBYTE *buf, UWORD x, UWORD y) {
    UWORD shift = x & 0x0f;
    UBYTE *dst = ((UBYTE*)buf) + y * screenByteWidth * depth + ((x >> 3)& 0xffe);
    WaitBlt();
    custom->bltcon0 = 0xe2 | SRCA | SRCB | SRCC | DEST | ( shift << ASHIFTSHIFT);
    custom->bltcon1 = shift << BSHIFTSHIFT;
    custom->bltapt = (APTR)bob;
    custom->bltbpt = (APTR)bob + bobByteWidth;
    custom->bltcpt = dst;
    custom->bltdpt = dst;
    custom->bltsize = (((bobHeight * depth)<< HSIZEBITS) ) | ((bobByteWidth / 2));    
}

static void BlitCircle2(UBYTE *buf, UWORD x, UWORD y) {
    UWORD shift = x & 0x0f;
    UBYTE *bob2 = (UBYTE *)bob + (bobByteWidth * bobHeight * bobDepth * 2);
    UBYTE *dst = ((UBYTE*)buf) + y * screenByteWidth * depth + ((x >> 3)& 0xffe);
    WaitBlt();
    custom->bltcon0 = 0xe2 | SRCA | SRCB | SRCC | DEST | ( shift << ASHIFTSHIFT);
    custom->bltcon1 = shift << BSHIFTSHIFT;
    custom->bltapt = (APTR)bob2;
    custom->bltbpt = (APTR)bob2 + bobByteWidth;
    custom->bltcpt = dst;
    custom->bltdpt = dst;
    custom->bltsize = (((bobHeight * depth)<< HSIZEBITS) ) | ((bobByteWidth / 2));    
}

// Color rotation 6>12>3>7>15>14>13>10>4>8>0
// 2>4
// 9>3
// 5>11>7

static void FadeOut(UBYTE *src, UBYTE *dst) {
    WORD bltSize = ((fadeOutHeight- vertSpeed) << HSIZEBITS) | screenByteWidth/2;    
    WORD imageMod = (depth-1) * screenByteWidth;

    WaitBlt();
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltcon1 = 0;
    // Copy 3 planes (0,1,2) over to the destination buffer (1,2,3)
    custom->bltcon0 = 0xf0 | SRCA | DEST;
    custom->bltapt = src;
    custom->bltamod = screenByteWidth;
    custom->bltdpt = dst + screenByteWidth + (screenByteWidth * depth * vertSpeed);
    custom->bltdmod = screenByteWidth;
    custom->bltsize = ((fadeOutHeight- vertSpeed) << HSIZEBITS) | ((screenByteWidth/2) * 3);
    WaitBlt();
    // Calculate the last plane    
    // plane 0 in dest = minterm * bit 1 A, bit 2 B, bit 3 C
    custom->bltcon0 = 0x94 | SRCA | SRCB | SRCC | DEST;
    custom->bltapt = src + screenByteWidth * 1;
    custom->bltamod = imageMod;
    custom->bltbpt = src + screenByteWidth * 2;
    custom->bltbmod = imageMod;
    custom->bltcpt = src + screenByteWidth * 3;
    custom->bltcmod = imageMod;
    custom->bltdpt = dst + (screenByteWidth * depth * vertSpeed);
    custom->bltdmod = imageMod;
    custom->bltsize = bltSize;
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

// Exported functions

void Sub_InitEffect() {
    buf0 = AllocMem(screenSize*2, MEMF_CHIP | MEMF_CLEAR);
    buf1 = AllocMem(screenSize*2, MEMF_CHIP | MEMF_CLEAR);
    buf = buf0;
    custom->dmacon = DMAF_SETCLR | DMAF_BLITHOG;
   	SetupScreen(buf0, 4);
}


void Sub_FreeEffect() {
    FreeMem(buf0, screenSize);
    FreeMem(buf1, screenSize);
    CleanupScreen();
}

UWORD frame = 0;
UWORD dir = 1;

BOOL Sub_CalcEffect(BOOL exit) {
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

    FadeOut(frontBuf, buf);


    frame+=dir;
    if ( frame >= 70 || frame <= 0) dir = -dir;
    UWORD pos = frame;    
    UWORD sin = sinus256[pos&0x3f];    
    UWORD sin2 = sinus32[pos&0x3f];    
    UWORD cos = sinus32[(pos+16)&0x3f];    

    PrepBlitCircle();

    BlitCircle(buf, (UWORD)0+sin, (UWORD)0+cos + 4);
    BlitCircle(buf, (UWORD)256-sin, (UWORD)0+cos + 4);
    BlitCircle(buf, (UWORD)96+sin2, (UWORD)32-cos + 4);
    BlitCircle(buf, (UWORD)160-sin2, (UWORD)32-cos + 4);
    BlitCircle(buf, (UWORD)0+sin, (UWORD)100+cos + 4);
    BlitCircle(buf, (UWORD)256-sin, (UWORD)100+ -cos + 4);
    UWORD co = frame & 0x3f;
    cos = sinus32[(pos+0)&0x3f];
    BlitCircle(buf, (UWORD)co, (UWORD)100+ cos);
    BlitCircle2(buf, (UWORD)co+196, (UWORD)60+ cos);
    cos = sinus32[(pos+16)&0x3f];
    BlitCircle(buf, (UWORD)co+64, (UWORD)100+ cos);
    BlitCircle2(buf, (UWORD)co, (UWORD)60+ cos);
    cos = sinus32[(pos+32)&0x3f];
    BlitCircle(buf, (UWORD)co+128, (UWORD)100+ cos);
    BlitCircle2(buf, (UWORD)co+64, (UWORD)60+ cos);
    cos = sinus32[(pos+48)&0x3f];
    BlitCircle(buf, (UWORD)co+196, (UWORD)100+ cos);    
    BlitCircle2(buf, (UWORD)co+128, (UWORD)60+ cos);
    //BlitCircle(buf, co, co);
    return TRUE;
}