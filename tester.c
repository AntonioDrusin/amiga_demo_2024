#include "support/gcc8_c_support.h"
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdarg.h>

typedef struct BlitterData {
    UWORD bltafwm;
    UWORD bltalwm;
    UWORD src;
    UWORD dst;
    UWORD ashift;
    UWORD bshift;
    UWORD size;
    BOOL descending;
} BlitterData;


static BlitterData dest;
static char buf[256];
static UWORD pos = 0;

void PrintPutChar() {
    register volatile UBYTE input ASM("d0");
    __asm volatile (
        "movem.l %%d0-%%d7/%%a0-%%a6,-(%%sp)\n"
        : 
        :
        : "cc", "memory");
    char c = input;
    buf[pos++] = c;    
    __asm volatile (
    "movem.l (%%sp)+,%%d0-%%d7/%%a0-%%a6"
        : 
        :
        : "cc", "memory");
}

void PrintF(const char* fmt, ...) {
    pos = 0;
	va_list vl;
	va_start(vl, fmt);
    RawDoFmt(fmt, vl, (void (*)()) PrintPutChar, 0);
    Write(Output(), buf, pos-1);
}

static void Verify(BlitterData expected) {
    BOOL failed = FALSE;
    
    if ( dest.dst != expected.dst ) {
        PrintF("\n dst expected %lx, was %lx", (ULONG)expected.dst, (ULONG)dest.dst);
        failed = TRUE;
    }
    if ( dest.ashift != expected.ashift ) {
        PrintF("\n ashift expected %lx, was %lx", (ULONG)expected.ashift, (ULONG)dest.ashift);
        failed = TRUE;
    }
    if ( dest.bshift != expected.bshift ) {
        PrintF("\n bshift expected %lx, was %lx", (ULONG)expected.bshift, (ULONG)dest.bshift);
        failed = TRUE;
    }
    if ( dest.src != expected.src ) {
        PrintF("\n src expected %lx, was %lx", (ULONG)expected.src, (ULONG)dest.src);
        failed = TRUE;
    }
    if ( dest.size != expected.size ) {
        PrintF("\n size expected %lx, was %lx", (ULONG)expected.size, (ULONG)dest.size);
        failed = TRUE;
    }
    if ( dest.descending != expected.descending ) {
        PrintF("\n descending expected %ld, was %ld", (ULONG)expected.descending, (ULONG)dest.descending);
        failed = TRUE;
    }
    if ( dest.size > 2 ) {
        if ( dest.bltafwm != expected.bltafwm ) {
            PrintF("\n bltafwm expected %lx, was %lx", (ULONG)expected.bltafwm, (ULONG)dest.bltafwm);
            failed = TRUE;
        }
        if ( dest.bltalwm != expected.bltalwm ) {
            PrintF("\n bltalwm expected %lx, was %lx", (ULONG)expected.bltalwm, (ULONG)dest.bltalwm);
            failed = TRUE;
        }
    }
    else {
        if ( (dest.bltafwm & dest.bltalwm) != (expected.bltafwm & expected.bltalwm)) {
            PrintF("\n Masks expected %lx, was %lx", (ULONG)(expected.bltafwm & expected.bltalwm), (ULONG)(dest.bltafwm & dest.bltalwm));
            PrintF("\n Masks f:%lx l:%lx", (ULONG)(dest.bltafwm),(ULONG)dest.bltalwm);
            failed = TRUE;
        }
    }

    if ( failed ) {
        PrintF(" FAILED\n");
        Exit(0);
    }
    else {
        PrintF(" SUCCESS\n");
    }
}

static void Calculate(UWORD x, UWORD dx, UWORD width ) {
    if ( !width ) return;

    if ( (dx & 0x0f) < (x & 0x0f) ) {
        // descending, it never expands the number of words
        UWORD srcSize = ((UWORD)((UWORD)(x+width-(UWORD)1) >> 4 ) - (UWORD)(x >> 4)) * (UWORD)2 + (UWORD)2;
        UWORD shift = (UWORD)(x - dx) & (UWORD)0x0f;

        UWORD startMaskShift = x & 0x0f;
        UWORD endMaskShift = ((UWORD)(x+width) & 0x0f)-1;
        dest.src = ((UWORD)(x+width-1) / 16) * 2;
        dest.dst = (((UWORD)(dx) / 16) * 2) + srcSize - 2;
        dest.bltafwm = (UWORD)((WORD)0x8000 >> endMaskShift);
        dest.bltalwm = (UWORD)0xffff >> startMaskShift;
        dest.size = srcSize;
        dest.ashift = shift;
        dest.bshift = shift;

        dest.descending = TRUE;
    }
    else {
        // ascending
        UWORD srcSize = ((UWORD)((UWORD)(x+width-1) >> 4 ) - (UWORD)(x >> 4)) * (UWORD)2 + (UWORD)2;
        UWORD dstSize = ((UWORD)((UWORD)(dx+width-1) >> 4 ) - (UWORD)(dx >> 4)) * (UWORD)2 + (UWORD)2;
        UWORD shift = (UWORD)(dx - x) & 0x0f;

        dest.src = (UWORD)(x / 16) * 2;
        dest.dst = (UWORD)(dx / 16) * 2;
        dest.bshift = shift;
        UWORD startMaskShift, endMaskShift;
        if ( dstSize > srcSize ) { // Mask the destination
            startMaskShift = (UWORD)(dx & 0x0f);
            endMaskShift = (UWORD)((UWORD)(dx+width) & 0x0f)-1;

            dest.size = dstSize;
            dest.ashift = 0;
        }
        else { // Mask the source
            startMaskShift = x & 0x0f;
            endMaskShift = (UWORD)((UWORD)(x+width) & 0x0f)-1;
            dest.size = srcSize;
            dest.ashift = shift; 
        }
        dest.bltafwm = (UWORD)0xffff >> startMaskShift;
        dest.bltalwm = (WORD)0x8000 >> endMaskShift;
        dest.descending = FALSE;
    }
}


static void Test(UWORD x, UWORD dx, UWORD width) {
    PrintF("Move from %ld to %ld, width: %ld", (LONG)x, (LONG)dx, (LONG)width);
    Calculate(x,dx,width);
}

void Tester() {
	BlitterData expected;
    Test(0, 16, 4); // From, To, Width
    expected.bltafwm = 0xf000;
    expected.bltalwm = 0xf000;
    expected.dst = 2;
    expected.src = 0;
    expected.ashift = 0;
    expected.bshift = 0;
    expected.size = 2;
    expected.descending = FALSE;
    Verify(expected);

    // Forward 
    Test(0, 4, 4); // From, To, Width
    expected.bltafwm = 0xf000;
    expected.bltalwm = 0xf000;
    expected.dst = 0;
    expected.src = 0;
    expected.ashift = 4;
    expected.bshift = 4;
    expected.size = 2;
    expected.descending = FALSE;
    Verify(expected);

    Test(2, 34, 28); // From, To, Width
    expected.bltafwm = 0x3fff;
    expected.bltalwm = 0xfffc;
    expected.dst = 4;
    expected.src = 0;
    expected.ashift = 0;
    expected.bshift = 0;
    expected.size = 4;
    expected.descending = FALSE;
    Verify(expected);

    Test(2, 38, 30); // From, To, Width
    expected.bltafwm = 0x03ff;
    expected.bltalwm = 0xf000;
    expected.dst = 4;
    expected.src = 0;
    expected.ashift = 0;
    expected.bshift = 4;
    expected.size = 6;
    expected.descending = FALSE;
    Verify(expected);

    Test(2, 42, 28);
    expected.bltafwm = 0x003f;
    expected.bltalwm = 0xfc00;
    expected.dst = 4;
    expected.src = 0;
    expected.ashift = 0;
    expected.bshift = 8;
    expected.size = 6;
    expected.descending = FALSE;
    Verify(expected);

    Test(0, 32, 32); // From, To, Width
    expected.bltafwm = 0xffff;
    expected.bltalwm = 0xffff;
    expected.dst = 4;
    expected.src = 0;
    expected.ashift = 0;
    expected.bshift = 0;
    expected.size = 4;
    expected.descending = FALSE;
    Verify(expected);

    // Backwards but forward or zero shift
    Test(24, 8, 8);
    expected.bltafwm = 0x00ff;
    expected.bltalwm = 0x00ff;
    expected.dst = 0;
    expected.src = 2;
    expected.ashift = 0;
    expected.bshift = 0;
    expected.size = 2;
    expected.descending = FALSE;
    Verify(expected);

    Test(50, 22, 28);
    expected.bltafwm = 0x03ff;
    expected.bltalwm = 0xc000;
    expected.dst = 2;
    expected.src = 6;
    expected.ashift = 0;
    expected.bshift = 4;
    expected.size = 6;
    expected.descending = FALSE;
    Verify(expected);

    Test(50, 20, 28);
    expected.bltafwm = 0x3fff;
    expected.bltalwm = 0xfffc;
    expected.dst = 2;
    expected.src = 6;
    expected.ashift = 2;
    expected.bshift = 2;
    expected.size = 4;
    expected.descending = FALSE;
    Verify(expected);

    // Forwards 2->3 words
    // 0011 1111 1111 1111 | 1111 1111 1111 1100 | 0000 0000 0000 0000 | 0000 0000 0000 0000 |
    // 0000 0000 0000 0000 | 0000 0000 0000 0011 | 1111 1111 1111 1111 | 1111 1111 1100 0000 |
    Test(2,30,28);
    expected.bltafwm = 0x0003;
    expected.bltalwm = 0xffc0;
    expected.dst = 2;
    expected.src = 0;
    expected.ashift = 0;
    expected.bshift = 12;
    expected.size = 6;
    expected.descending = FALSE;
    Verify(expected);

    // backwards and backwards shift    
    // 0000 0000 1111 1111 | 0000 0000 0000 0000
    // 0000 1111 1111 0000 | 0000 0000 0000 0000
    Test(8, 4, 8);
    expected.bltafwm = 0x00ff;
    expected.bltalwm = 0xffff;
    expected.dst = 0;
    expected.src = 0;
    expected.ashift = 4;
    expected.bshift = 4;
    expected.size = 2;
    expected.descending = TRUE;
    Verify(expected);

    // 0000 0000 0000 0000 | 0111 1111 1111 1111 | 1111 1111 1111 1111 | 1111 1100 0000 0000 |
    // 1111 1111 1111 1111 | 1111 1111 1111 1111 | 1111 1000 0000 0000 | 0000 0000 0000 0000 |
    Test(17, 0, 37);
    expected.bltafwm = 0xfc00;
    expected.bltalwm = 0x7fff;
    expected.dst = 4;
    expected.src = 6;
    expected.ashift = 1;
    expected.bshift = 1;
    expected.size = 6;
    expected.descending = TRUE;
    Verify(expected);

    // 0000 0000 0000 0000 | 0111 1111 1111 1111 | 1111 1111 1111 1111 | 1111 1100 0000 0000 |
    // 0000 0000 0000 0000 | 0000 0000 0000 0000 | 1111 1111 1111 1111 | 1111 1111 1111 1111 | 1111 1000 0000 0000 | 0000 0000 0000 0000 |
    Test(17, 32, 37);
    expected.bltafwm = 0xfc00;
    expected.bltalwm = 0x7fff;
    expected.dst = 8;
    expected.src = 6;
    expected.ashift = 1;
    expected.bshift = 1;
    expected.size = 6;
    expected.descending = TRUE;
    Verify(expected);
    
    // 0011 1111 1111 1111 | 1111 1111 1111 1111 | 1100 0000 0000 0000 | 0000 0000 0000 0000 |
    // 0000 0000 0000 0000 | 0000 0000 0000 0001 | 1111 1111 1111 1111 | 1111 1111 1111 1110 |
    Test(2, 31, 32); // From, To, Width
    expected.bltafwm = 0x3fff;
    expected.bltalwm = 0xc000;
    expected.dst = 2;
    expected.src = 0;
    expected.ashift = 13;
    expected.bshift = 13;
    expected.size = 6;
    expected.descending = FALSE;
    Verify(expected);

    // 0000 1111 1111 1111 | 1111 1111 1111 1111 | 1100 0000 0000 0000 | 0000 0000 0000 0000 |
    // 0111 1111 1111 1111 | 1111 1111 1111 1110 | 0000 0000 0000 0000 | 0000 0000 0000 0000 |
    Test(4, 1, 30); // From, To, Width
    expected.bltafwm = 0xc000;
    expected.bltalwm = 0x0fff;
    expected.dst = 4;
    expected.src = 4;
    expected.ashift = 3;
    expected.bshift = 3;
    expected.size = 6;
    expected.descending = TRUE;
    Verify(expected);


    // 0011 1111 1111 1111 | 1111 1111 1111 1111 | 1100 0000 0000 0000 | 0000 0000 0000 0000 | 0000 0000 0000 0000 |
    // 0000 0000 0000 0000 | 0000 0000 0000 0000 | 0111 1111 1111 1111 | 1111 1111 1111 1111 | 1000 0000 0000 0000 |
    Test(2, 33, 32); // From, To, Width
    expected.bltafwm = 0xc000;
    expected.bltalwm = 0x3fff;
    expected.dst = 8;
    expected.src = 4;
    expected.ashift = 1;
    expected.bshift = 1;
    expected.size = 6;
    expected.descending = TRUE;
    Verify(expected);



    // 0000 0000 0000 0000 | 0011 1111 1111 1111 | 1111 1111 1111 1111 | 1100 0000 0000 0000 | 
    // 0111 1111 1111 1111 | 1111 1111 1111 1111 | 1000 0000 0000 0000 | 0000 0000 0000 0000 |
    Test(18, 1, 32); // From, To, Width
    expected.bltafwm = 0xc000;
    expected.bltalwm = 0x3fff;
    expected.dst = 4;
    expected.src = 6;
    expected.ashift = 1;
    expected.bshift = 1;
    expected.size = 6;
    expected.descending = TRUE;
    Verify(expected);

    // backward shift from 2 to 3 words NEVER HAPPENS    	
}

