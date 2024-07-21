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
static const UWORD displayWidth = 320;
static const UWORD displayHeight = 100;
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
//    custom->dmacon = DMAF_SETCLR | DMAF_BLITHOG;
	UWORD tst=*(volatile UWORD*)&custom->dmaconr; //for compatiblity a1000
	(void)tst;
	while (*(volatile UWORD*)&custom->dmaconr&(1<<14)) {} //blitter busy wait
//    custom->dmacon = DMAF_BLITHOG;
}

static ULONG v = 12323;
static ULONG u = 3321;

static ULONG random() {
    v = 36969*(v & 65535) + (v >> 16);
    u = 18000*(u & 65535) + (u >> 16);
    return (v << 16) + (u & 65535);
}


static const UWORD bobByteWidth = 6;
static const UWORD bobHeight = 32;
static const UWORD bobDepth = 4;

static UWORD mask = 0xaaaa;

__attribute__((always_inline)) inline static void Smudge(UBYTE *srcBuf, UBYTE *dstBuf, UWORD x, UWORD y, UWORD width, UWORD height, UWORD dx, UWORD dy) {
    if ( ((UWORD)(dx & 0x0f)) < ((UWORD)(x & 0x0f ))) {
        
        // descending never expands number of words
        UWORD srcSize = ((UWORD)((UWORD)(x+width-(UWORD)1) >> 4 ) - (UWORD)(x >> 4)) * (UWORD)2 + (UWORD)2;
        UWORD size = srcSize;
        UWORD shift = (UWORD)(x - dx) & (UWORD)0x0f;
        UWORD startMaskShift = x & 0x0f;
        UWORD endMaskShift = ((UWORD)(x+width) & 0x0f)-1;
        UWORD srcOffset = ((UWORD)(x+width-1) / 16) * 2;
        UWORD dstOffset = (((UWORD)(dx) / 16) * 2) + size - 2;
        UWORD firstMask = (UWORD)((WORD)0x8000 >> endMaskShift);
        UWORD lastMask = (UWORD)0xffff >> startMaskShift;

        UBYTE *src = srcBuf + srcOffset + (y+(height-1)) * depth * screenByteWidth; 
        UBYTE *dst = dstBuf + dstOffset + (dy+(height-1)) * depth * screenByteWidth;

        WaitBlt();
        // Calculate the last plane    
        // plane 0 in dest = minterm * bit 1 A, bit 2 B, bit 3 C
        custom->bltcon0 = 0x94 | SRCA | SRCB | SRCC | DEST;
        custom->bltcon1 = 0 | BLITREVERSE;
        custom->bltafwm = 
        custom->bltalwm = 0xffff;
        custom->bltapt = src + screenByteWidth * 1;
        custom->bltbpt = src + screenByteWidth * 2;
        custom->bltcpt = src + screenByteWidth * 3;
        custom->bltamod = 
        custom->bltbmod = 
        custom->bltcmod = screenByteWidth * depth - size;
        custom->bltdmod = 0;
        custom->bltdpt = carry + carrySize - 2;
        custom->bltsize = ((height) << HSIZEBITS) | (size/2);
        
        WaitBlt();
        custom->bltafwm = firstMask & mask;
        custom->bltalwm = lastMask & mask;
        custom->bltadat = mask;
        custom->bltcon0 = 0xca | SRCB | SRCC | DEST | (shift << 12);
        custom->bltcon1 = (shift << 12) | BLITREVERSE;
        // SAME
        // custom->bltbmod = 
        // custom->bltcmod = 
        custom->bltdmod = screenByteWidth * depth - size;

        UBYTE *shiftSrc = src + screenByteWidth * (depth-2);
        UBYTE *shiftDst = dst + screenByteWidth * (depth-1);
        for ( int i=0; i<=2; i++ ) {
            custom->bltbpt = shiftSrc;
            custom->bltcpt = 
            custom->bltdpt = shiftDst;
            custom->bltsize = (height << HSIZEBITS) | (size/2);
            shiftSrc -= screenByteWidth;
            shiftDst -= screenByteWidth;
            WaitBlt();    
        }    

        // Now move the results to bitplane 0 with masking and shifting
        // SAME custom->bltcon0 = 0xca | SRCB | SRCC | DEST | (shift << 12);
        // SAME custom->bltcon1 = (shift << 12) | BLITREVERSE;
        custom->bltbmod = 0;
        // SAME custom->bltcmod = 
        // SAME custom->bltdmod = screenByteWidth * depth - size;
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

        WaitBlt();
        // Calculate the last plane    
        // plane 0 in dest = minterm * bit 1 A, bit 2 B, bit 3 C
        custom->bltcon0 = 0x94 | SRCA | SRCB | SRCC | DEST;
        custom->bltcon1 = 0;
        custom->bltafwm = 
        custom->bltalwm = 0xffff;
        custom->bltapt = src + screenByteWidth * 1;
        custom->bltbpt = src + screenByteWidth * 2;
        custom->bltcpt = src + screenByteWidth * 3;
        custom->bltamod = screenByteWidth * depth - size;
        custom->bltbmod = screenByteWidth * depth - size;
        custom->bltcmod = screenByteWidth * depth - size;
        custom->bltdpt = carry;
        custom->bltdmod = 0;
        custom->bltsize = ((height) << HSIZEBITS) | (size/2);
        
        WaitBlt();
        custom->bltafwm = firstMask & mask;
        custom->bltalwm = lastMask & mask;
        custom->bltadat = mask;
        custom->bltcon0 = 0xca | SRCB | SRCC | DEST | (aShift << 12);
        custom->bltcon1 = (bShift << 12);
        // SAME 
        // custom->bltbmod = 
        // custom->bltcmod = 
        custom->bltdmod = screenByteWidth * depth - size;

        // Shift the last plane down first, in case we target the same buffer
        UBYTE *shiftSrc = src + screenByteWidth * (depth-2);
        UBYTE *shiftDst = dst + screenByteWidth * (depth-1);
        for ( int i=0; i<=2; i++ ) {
            custom->bltbpt = shiftSrc;
            custom->bltcpt = 
            custom->bltdpt = shiftDst;
            custom->bltsize = (height << HSIZEBITS) | (size/2);
            shiftSrc -= screenByteWidth;
            shiftDst -= screenByteWidth;
            WaitBlt();    
        }    

        // Now move the results to bitplane 0 with masking and shifting
        // SAME custom->bltcon0 = 0xca | SRCB | SRCC | DEST | (aShift << 12);
        // SAME custom->bltcon1 = (bShift << 12);
        custom->bltbmod = 0;
        // SAME
        // custom->bltcmod = 
        // custom->bltdmod = screenByteWidth * depth - size;
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

void CopyBuf ( BYTE *src, BYTE *dst, UWORD height) {

    WaitBlt();
    custom->bltcon0 = 0xf0 | SRCA | DEST;
    custom->bltcon1 = 0;
    custom->bltapt = src;
    custom->bltdpt = dst;
    custom->bltamod =
    custom->bltdmod = 0;
    custom->bltafwm =
    custom->bltalwm = 0xffff;
    custom->bltsize = (UWORD)((((UWORD)height * (UWORD)depth) << HSIZEBITS) | (screenByteWidth/2));

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
    WaitBlt();
    //custom->dmacon = DMAF_BLITHOG;
   	SetupScreenComplete(buf0, 4, displayWidth, displayHeight);
    mask = 0xaaaa;
}

void ShaderBob_InitEffect() {
    buf0 = AllocMem(screenSize, MEMF_CHIP | MEMF_CLEAR);
    buf1 = AllocMem(screenSize, MEMF_CHIP | MEMF_CLEAR);
    carry = AllocMem(carrySize, MEMF_CHIP | MEMF_CLEAR);
    buf = buf0;
    custom->dmacon = DMAF_SETCLR | DMAF_BLITHOG;
    BlitterBox(buf0, 0, 0, 320, 256, 3);
    BlitterBox(buf1, 0, 0, 320, 256, 3);
    WaitBlt();
   	SetupScreen(buf0, 4);
    mask = 0xffff;
}


void Fire_FreeEffect() {
    FreeMem(buf0, screenSize);
    FreeMem(buf1, screenSize);
    FreeMem(carry, carrySize);
    CleanupScreen();
}



static UWORD frame = 0;
static WORD px = 75;
static WORD py = 67;
static WORD dx = 2;
static WORD dy = 2;
static WORD pw = 22;
static WORD ph = 22;


static const WORD wave0[] = {
    -1,-3,-1,0,
    -2,0,1,1,
    2,1,0,2,
    0,3,0,-1
    };

static const WORD wave0b[] = {
    -2,-3,-2,-1,
    -2,-1,2,1,
    2,1,1,2,
    1,3,4,-1
    };    

static const WORD wave1[] = {
    1,3,1,2,
    2,3,1,1,
    2,2,1,2,
    4,3,3,1
};
static const WORD wave1b[] = {
    2,3,1,5,
    2,3,2,1,
    2,1,3,1,
    4,2,1,1
};

WORD getWaveY() {
    static UWORD ix = 0;
    if ( frame & 0x04 ) ix++;
    return (wave1[ix & 0x0f])*3;

}

WORD getWaveX() {
    static UWORD ix = 0;
    if ( frame % 7 == 0 ) ix++;
    return (wave0[ix & 0x0f]);
}

void FireWaitLine(USHORT line) {
	while (1) {
		volatile ULONG vpos=*(volatile ULONG*)0xDFF004;
		if(((vpos >> 8) & 511) == line)
			break;
	}
}


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

    FireWaitLine(0x0);   
    

    mask = mask << 1 | (mask & 0x8000 ? 1:0);
    //mask = 0xffff;

    // 3 boxes at the bottom
    // 20px - 280px - 20px;
    const UWORD width = 304;
    const UWORD margin = (320-width)/2;
    const UWORD heightJump = 17;
    const UWORD h = 27;
    UWORD x;
    UWORD w;

    CopyBuf(frontBuf, buf, 90 + heightJump - 8);
    UWORD y = 90;


    w = width / 3;    
    x = margin;
    for ( UWORD i=0; i<3; i++) {        
        Smudge(buf, buf, x, y, w, h, x + getWaveX(), y-getWaveY());
        x += w;
    }

    // 6 boxes above that
    y -= heightJump;
    w = (width) / 6;
    x = margin;
    for ( UWORD i=0; i<6; i++) {
        Smudge(buf, buf, x, y, w, h, x + getWaveX(), y-getWaveY());
        x += w;
    }
     
    // 12 boxes above those
    y -= heightJump;
    w = (width) / 12;
    x = margin;
    for ( UWORD i=0; i<12; i++) {
        Smudge(buf, buf, x, y, w, h, x + getWaveX(), y-getWaveY());
        x += w;
    }

    // 24 boxes above those
    y -= heightJump*2;
    w = (width) / 24;
    x = margin;
    for ( UWORD i=0; i<25; i++) {
        Smudge(buf, buf, x, y, w, h*2, x + getWaveX(), y-getWaveY());
        x += w;
    }


    BlitterBox(buf, 0, 90+10, 320, heightJump, 3);


    frame++;
}

BOOL ShaderBob_CalcEffect(BOOL exit) {
    px += dx;
    py += dy;
    if ( px < 0 || px+pw > 320) { dx=-dx; px += dx; if ( dx > 0 )  dx++; else dx--;};
    if ( py < 0 || py+ph > 256) { dy=-dy; py += dy; if ( dy > 0 )  dy++; else dy--;};
    if ( frame++ & 0x100) {
        if ( dx < 0 )  dx=-1;
        if ( dy < 0 )  dy=-1;
    }
    mask = mask << 1 | (mask & 0x8000 ? 1:0);
    Smudge(buf, buf, px, py, pw, ph, px, py);
    Smudge(buf, buf, 320-px-pw, py, pw, ph, 320-px-pw, py);
    Smudge(buf, buf, px, 256-py-ph, pw, ph, px, 256-py-ph);
    Smudge(buf, buf, 320-px-pw, 256-py-ph, pw, ph, 320-px-pw, 256-py-ph);
    
    if ( frame & 0x400) {
        WaitBlt();
        BlitterBox(buf, 0, 0, 320, 256, 3);
        WaitBlt();
        frame = 0;
    }
    
    //WaitBlt();
    frame++;
}