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
static ULONG screenSize = (screenWidth/8) * screenHeight * (depth); // One extra depth for carry
static ULONG carrySize = (screenWidth/8) * screenHeight;

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

    if ( ((UWORD)(dx & 0x0f)) < ((UWORD)(x & 0x0f ))) {
        // descending never expands number of words
        UWORD srcSize = ((UWORD)((UWORD)(x+width-(UWORD)1) >> 4 ) - (UWORD)(x >> 4)) * (UWORD)2 + (UWORD)2;
        UWORD size = srcSize;
        UWORD shift = (UWORD)(x - dx) & (UWORD)0x0f;
        UWORD startMaskShift = x & 0x0f;
        UWORD endMaskShift = ((UWORD)(x+width) & 0x0f)-1;
        UWORD srcOffset = ((UWORD)(x+width-1) / 16) * 2;
        UWORD dstOffset = ((UWORD)(dx+width-1) / 16) * 2;
        UWORD firstMask = (UWORD)((WORD)0x8000 >> endMaskShift);
        UWORD lastMask = (UWORD)0xffff >> startMaskShift;

        UBYTE *src = srcBuf + srcOffset + (y+(height-1)) * depth * screenByteWidth; 
        UBYTE *dst = dstBuf + dstOffset + (dy+(height-1)) * depth * screenByteWidth;
        UBYTE *shiftSrc = src;
        UBYTE *shiftDst = dst + screenByteWidth;

        WaitBlt();
        custom->bltafwm = firstMask;
        custom->bltalwm = lastMask;
        custom->bltadat = 0xffff;
        custom->bltcon0 = 0xca | SRCB | SRCC | DEST | (shift << 12);
        custom->bltcon1 = (shift << 12) | BLITREVERSE;
        custom->bltbmod = 
        custom->bltcmod = 
        custom->bltdmod = screenByteWidth * depth - size;

        for ( int i=0; i<=2; i++ ) {
            custom->bltbpt = shiftSrc;
            custom->bltcpt = 
            custom->bltdpt = shiftDst;
            custom->bltsize = (height << HSIZEBITS) | (size/2);
            shiftSrc += screenByteWidth;
            shiftDst += screenByteWidth;
            WaitBlt();    
        }    

        // Calculate the last plane    
        // plane 0 in dest = minterm * bit 1 A, bit 2 B, bit 3 C
        custom->bltcon0 = 0x94 | SRCA | SRCB | SRCC | DEST;
        custom->bltcon1 = 0 | BLITREVERSE;
        custom->bltapt = src + screenByteWidth * 1;
        custom->bltamod = screenByteWidth * depth - size;
        custom->bltbpt = src + screenByteWidth * 2;
        custom->bltbmod = screenByteWidth * depth - size;
        custom->bltcpt = src + screenByteWidth * 3;
        custom->bltcmod = screenByteWidth * depth - size;
        custom->bltdpt = carry + carrySize - 2;
        custom->bltdmod = 0;
        custom->bltsize = ((height) << HSIZEBITS) | (size/2);
        WaitBlt();

        // Now move the results to bitplane 0 with masking and shifting
        custom->bltafwm = firstMask;
        custom->bltalwm = lastMask;
        custom->bltadat = 0xffff;
        custom->bltcon0 = 0xca | SRCB | SRCC | DEST | (shift << 12);
        custom->bltcon1 = (shift << 12) | BLITREVERSE;
        custom->bltbmod = 0;
        custom->bltcmod = 
        custom->bltdmod = screenByteWidth * depth - size;
        custom->bltbpt = carry + carrySize - 2;
        custom->bltcpt = 
        custom->bltdpt = dst;
        custom->bltsize = (height << HSIZEBITS) | (size/2);

    }
    else {
        UWORD srcSize = ((UWORD)((UWORD)(x+width-(UWORD)1) >> 4 ) - (UWORD)(x >> 4)) * (UWORD)2 + (UWORD)2;
        UWORD dstSize = ((UWORD)((UWORD)(dx+width-1) >> 4 ) - (UWORD)(dx >> 4)) * (UWORD)2 + (UWORD)2;
        UWORD shift = (UWORD)(dx - x) & 0x0f;
        UWORD srcOffset = (UWORD)(x / 16) * 2;
        UWORD dstOffset = (UWORD)(dx / 16) * 2;
        UWORD size;
        UWORD startMaskShift, endMaskShift;
        UWORD aShift, bShift;

        bShift = shift;
        if ( dstSize > srcSize ) { // mask destination no mask shift
            startMaskShift = (UWORD)(dx & 0x0f);
            endMaskShift = (UWORD)((UWORD)(dx+width) & 0x0f)-1;
            size = dstSize;
            aShift = 0;
        }
        else { // mask source and then shift
            startMaskShift = x & 0x0f;
            endMaskShift = (UWORD)((UWORD)(x+width) & 0x0f)-1;
            size = srcSize;
            aShift = shift;
        }
        UWORD firstMask = (UWORD)0xffff >> startMaskShift;
        UWORD lastMask = (WORD)0x8000 >> endMaskShift;

        UBYTE *src = srcBuf + srcOffset + y * depth * screenByteWidth; 
        UBYTE *dst = dstBuf + dstOffset + dy * depth * screenByteWidth;
        UBYTE *shiftSrc = src;
        UBYTE *shiftDst = dst + screenByteWidth;

        WaitBlt();
        custom->bltafwm = firstMask;
        custom->bltalwm = lastMask;
        custom->bltadat = 0xffff;
        custom->bltcon0 = 0xca | SRCB | SRCC | DEST | (aShift << 12);
        custom->bltcon1 = (bShift << 12);
        custom->bltbmod = 
        custom->bltcmod = 
        custom->bltdmod = screenByteWidth * depth - size;

        for ( int i=0; i<=2; i++ ) {
            custom->bltbpt = shiftSrc;
            custom->bltcpt = 
            custom->bltdpt = shiftDst;
            custom->bltsize = (height << HSIZEBITS) | (size/2);
            shiftSrc += screenByteWidth;
            shiftDst += screenByteWidth;
            WaitBlt();    
        }    

        // Calculate the last plane    
        // plane 0 in dest = minterm * bit 1 A, bit 2 B, bit 3 C
        custom->bltcon0 = 0x94 | SRCA | SRCB | SRCC | DEST;
        custom->bltcon1 = 0;
        custom->bltapt = src + screenByteWidth * 1;
        custom->bltamod = screenByteWidth * depth - size;
        custom->bltbpt = src + screenByteWidth * 2;
        custom->bltbmod = screenByteWidth * depth - size;
        custom->bltcpt = src + screenByteWidth * 3;
        custom->bltcmod = screenByteWidth * depth - size;
        custom->bltdpt = carry;
        custom->bltdmod = 0;
        custom->bltsize = ((height) << HSIZEBITS) | (size/2);
        WaitBlt();

        // Now move the results to bitplane 0 with masking and shifting
        custom->bltafwm = firstMask;
        custom->bltalwm = lastMask;
        custom->bltadat = 0xffff;
        custom->bltcon0 = 0xca | SRCB | SRCC | DEST | (aShift << 12);
        custom->bltcon1 = (bShift << 12);
        custom->bltbmod = 0;
        custom->bltcmod = 
        custom->bltdmod = screenByteWidth * depth - size;
        custom->bltbpt = carry;
        custom->bltcpt = 
        custom->bltdpt = dst;
        custom->bltsize = (height << HSIZEBITS) | (size/2);
    }
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


static const WORD wave_0_x[16] = {0,1,1,2,1,0,0,-1,-1,-2,-1,-0,0,-1,0};
static const WORD wave_1_x[16] = {0,2,1,4,1,2,1,-1,-4,-2,-1,-1,2,-1,1};
static const WORD wave_0_y[16] = {-1,-1,-1,-1,-1,-2,-1,-1,-1,-2,-2,-1,-1,-1,-1};


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
    


    frame = frame & 0x07f;
    //ScreenClear(buf);
    // x, y, w, h, dx, dy
    Smudge(frontBuf, buf, 
        50, 80, 
        100, 30, 
        50+ wave_0_x[frame&0x0f], 80 + wave_0_y[frame&0x0f]
    );
    Smudge(frontBuf, buf,
         60, 70,
         40, 20,
         61+ wave_1_x[frame&0x0f], 69
    );

    BlitterBox(buf, 60, 100, 80, 12, 3);
    frame++;
}