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

static const UWORD depth = 4;
static const UWORD screenWidth = 320;
static const UWORD screenByteWidth = screenWidth / 8;
static const UWORD screenHeight = 256;
static const UWORD vertSpeed = 4;

// Naive 15 color can do 46 lines.
static const UWORD fadeOutHeight = 195; 
static UBYTE *buf0;
static UBYTE *buf1;
static UBYTE *carry;
static UBYTE *buf;

__attribute__((always_inline)) inline void WaitBlt() {
	UWORD tst=*(volatile UWORD*)&custom->dmaconr; //for compatiblity a1000
	(void)tst;
	while (*(volatile UWORD*)&custom->dmaconr&(1<<14)) {} //blitter busy wait
}

// static ULONG v = 12323;
// static ULONG u = 3321;

// static ULONG random() {
//     v = 36969*(v & 65535) + (v >> 16);
//     u = 18000*(u & 65535) + (u >> 16);
//     return (v << 16) + (u & 65535);
// }


static const UWORD bobByteWidth = 6;
static const UWORD bobHeight = 32;
static const UWORD bobDepth = 4;


static void Smudge(UBYTE *srcBuf, UBYTE *dstBuf, UWORD x, UWORD y, UWORD width, UWORD height, UWORD dx, UWORD dy) {
    UBYTE *src = srcBuf + (x / (UWORD)16)*(UWORD)2 + y * depth * screenByteWidth; 
    UBYTE *dst = dstBuf + (dx / (UWORD)16)*(UWORD)2 + dy * depth * screenByteWidth;
    UWORD boxByteWidth = (((x+width)/16)-(x/16))*(UWORD)2+(UWORD)2;    
    UBYTE startOffset = x & (UWORD)0x0f;
    UBYTE endOffset = (x + width) & (UWORD)0x0f;


    UBYTE *shiftSrc = src;
    UBYTE *shiftDst = dst + screenByteWidth;

    // Copy 3 planes (0,1,2) over to the destination buffer (1,2,3)
    // A : mask
    // B : src
    // C : dst
    // D : dst

    UWORD shift = 1;
    WaitBlt();
    custom->bltafwm = 0xffff >> startOffset;
    custom->bltalwm = (WORD)0x8000 >> endOffset;
    custom->bltadat = 0xffff;
    custom->bltcon0 = 0xca | SRCB | SRCC | DEST | (shift << 12);
    custom->bltcon1 = (shift << 12);
    custom->bltbmod = 
    custom->bltcmod = 
    custom->bltdmod = screenByteWidth * depth - boxByteWidth;
    for ( int i=0; i<=2; i++ ) {
        custom->bltbpt = shiftSrc;
        custom->bltcpt = 
        custom->bltdpt = shiftDst;
        custom->bltsize = (height << HSIZEBITS) | (boxByteWidth/2);
        shiftSrc += screenByteWidth;
        shiftDst += screenByteWidth;
        WaitBlt();        
    }

    // Calculate the last plane    
    // plane 0 in dest = minterm * bit 1 A, bit 2 B, bit 3 C
    custom->bltcon0 = 0x94 | SRCA | SRCB | SRCC | DEST;
    custom->bltapt = src + screenByteWidth * 1;
    custom->bltamod = screenByteWidth * depth - boxByteWidth;
    custom->bltbpt = src + screenByteWidth * 2;
    custom->bltbmod = screenByteWidth * depth - boxByteWidth;
    custom->bltcpt = src + screenByteWidth * 3;
    custom->bltcmod = screenByteWidth * depth - boxByteWidth;
    custom->bltdpt = carry;
    custom->bltdmod = 0;
    custom->bltsize = ((height) << HSIZEBITS) | (boxByteWidth/2);
    WaitBlt();
    // Now move the results to bitplane 0 with masking and shifting
    custom->bltafwm = 0xffff >> startOffset;
    custom->bltalwm = (WORD)0x8000 >> endOffset;
    custom->bltadat = 0xffff;
    custom->bltcon0 = 0xca | SRCB | SRCC | DEST | (shift << 12);
    custom->bltcon1 = (shift << 12);
    custom->bltbmod = 0;
    custom->bltcmod = 
    custom->bltdmod = screenByteWidth * depth - boxByteWidth;
    custom->bltbpt = carry;
    custom->bltcpt = 
    custom->bltdpt = dst;
    custom->bltsize = (height << HSIZEBITS) | (boxByteWidth/2);

}

void BlitterBox(APTR buf, UWORD x, UWORD y, UWORD width, UWORD height, UWORD color) {
    APTR dst = buf + (x / (UWORD)16)*(UWORD)2 + y * depth * screenByteWidth; 
    UWORD boxByteWidth = (((x+width)/16)-(x/16))*(UWORD)2+(UWORD)2;    
    UBYTE startOffset = x & (UWORD)0x0f;
    UBYTE endOffset = (x + width) & (UWORD)0x0f;

    WaitBlt();
    custom->bltafwm = 0xffff >> startOffset;
    custom->bltalwm = (WORD)0x8000 >> endOffset;
    custom->bltadat = 0xffff;
    custom->bltcon1 = 0;
    custom->bltbmod = depth*screenByteWidth - boxByteWidth;
    custom->bltdmod = depth*screenByteWidth - boxByteWidth;
    for ( int p = 0; p<depth; p++ ) {
        if ( p ) WaitBlt();
        if (color & 0x1 != 0) {
            custom->bltcon0 = 0xfc | SRCB | DEST;
        }
        else {
            custom->bltcon0 = 0x0c | SRCB | DEST;
        }        
        custom->bltbpt = dst;
        custom->bltdpt = dst;
        custom->bltsize = ((height) << HSIZEBITS) | (boxByteWidth / 2) ;
        color = color >> 1;
        dst += screenByteWidth;
   }
}

void ScreenClear(APTR buf) {
    WaitBlt();
    custom->bltcon0 = 0x00 | DEST;
    custom->bltcon1 = 0;
    custom->bltafwm = 0xffff;
    custom->bltalwm = 0xffff;
    custom->bltdpt = buf;
    custom->bltdmod = 0;
    custom->bltsize = (UWORD)(((UWORD)screenHeight * (UWORD)depth) << HSIZEBITS) | ((UWORD)screenByteWidth / 2);
}

// Exported functions
static ULONG screenSize = (screenWidth/8) * screenHeight * (depth); // One extra depth for carry
static ULONG carrySize = (screenWidth/8) * screenHeight;
void Fire_InitEffect() {
    buf0 = AllocMem(screenSize, MEMF_CHIP | MEMF_CLEAR);
    buf1 = AllocMem(screenSize, MEMF_CHIP | MEMF_CLEAR);
    carry = AllocMem(carrySize, MEMF_CHIP | MEMF_CLEAR);
    buf = buf0;
    custom->dmacon = DMAF_SETCLR | DMAF_BLITHOG;
   	SetupScreen(buf0, 4);
}


void Fire_FreeEffect() {
    FreeMem(buf0, screenSize);
    FreeMem(buf1, screenSize);
    FreeMem(carry, carrySize);
    CleanupScreen();
}


static UWORD frame = 0;
BOOL Fire_CalcEffect(BOOL exit) {
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
    
    Smudge(frontBuf, buf, 50, 90, 100, 20, 51, 89);


    frame = frame & 0x07f;
    //ScreenClear(buf);
    BlitterBox(buf, 10, 100, 200, 12, 3);
    frame++;
}