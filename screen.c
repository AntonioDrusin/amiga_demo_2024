#include "support/gcc8_c_support.h"
#include <exec/execbase.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include "hardware.h"

const UWORD screenDepth = 4;
const UWORD lineSize = 320/8;
UWORD *copperPlanes;

// set up a 320x256 lowres display
__attribute__((always_inline)) inline USHORT* screenScanDefault(USHORT* copListEnd) {
	const USHORT x=129;
	const USHORT width=320;
	const USHORT height=256;
	const USHORT y=44;
	const USHORT RES=8; //8=lowres,4=hires
	USHORT xstop = x+width;
	USHORT ystop = y+height;
	USHORT fw=(x>>1)-RES;

	*copListEnd++ = offsetof(struct Custom, ddfstrt);
	*copListEnd++ = fw;
	*copListEnd++ = offsetof(struct Custom, ddfstop);
	*copListEnd++ = fw+(((width>>4)-1)<<3);
	*copListEnd++ = offsetof(struct Custom, diwstrt);
	*copListEnd++ = x+(y<<8);
	*copListEnd++ = offsetof(struct Custom, diwstop);
	*copListEnd++ = (xstop-256)+((ystop-256)<<8);
	return copListEnd;
}

__attribute__((always_inline)) inline USHORT* copSetPlanes(UBYTE bplPtrStart,USHORT* copListEnd,const UBYTE **planes,int numPlanes) {
	for (USHORT i=0;i<numPlanes;i++) {
		ULONG addr=(ULONG)planes[i];
		*copListEnd++=offsetof(struct Custom, bplpt[0]) + (i + bplPtrStart) * sizeof(APTR);
		*copListEnd++=(UWORD)(addr>>16);
		*copListEnd++=offsetof(struct Custom, bplpt[0]) + (i + bplPtrStart) * sizeof(APTR) + 2;
		*copListEnd++=(UWORD)addr;
	}
	return copListEnd;
}


__attribute__((always_inline)) inline USHORT* copSetColor(USHORT* copListCurrent,USHORT index,USHORT color) {
	*copListCurrent++=offsetof(struct Custom, color) + sizeof(UWORD) * index;
	*copListCurrent++=color;
	return copListCurrent;
}

// Color rotation 6>12>3>7>15>14>13>10>4>8>0
// 2>4
// 9>3
// 5>11>7
const UWORD colors[] = {
    0x000, // 0
    0xfee, // 1
    0x666, // 2
    0xccc, // 3
    0x555, // 4
    0x7f7, // 5
    0xfff, // 6
    0xbbb, // 7
    0x333, // 8
    0xddd, // 9
    0x777, // 10
    0x2e2, // 11
    0xeee, // 12
    0x888, // 13
    0x999, // 14
    0xaaa, // 15
};

void SetupScreen(APTR image) {
   	USHORT* copper1 = (USHORT*)AllocMem(1024, MEMF_CHIP);
	USHORT* copPtr = copper1;

  	// register graphics resources with WinUAE for nicer gfx debugger experience
	debug_register_copperlist(copper1, "copper1", 1024, 0);

	copPtr = screenScanDefault(copPtr);
	//enable bitplanes	
	*copPtr++ = offsetof(struct Custom, bplcon0);
	*copPtr++ = (0<<10)/*dual pf*/|(1<<9)/*color*/|((screenDepth)<<12)/*num bitplanes*/;
	*copPtr++ = offsetof(struct Custom, bplcon1);	//scrolling
	*copPtr++ = 0;
	*copPtr++ = offsetof(struct Custom, bplcon2);	//playfied priority
	*copPtr++ = 1<<6;//0x24;			//Sprites have priority over playfields

	//set bitplane modulo
	*copPtr++=offsetof(struct Custom, bpl1mod); //odd planes   1,3,5
	*copPtr++=(screenDepth-1)*lineSize;
	*copPtr++=offsetof(struct Custom, bpl2mod); //even  planes 2,4
	*copPtr++=(screenDepth-1)*lineSize;

    // Set a number of bitplanes
    copperPlanes = copPtr;
    const UBYTE* planes[screenDepth];    
    for(int a=0;a<screenDepth;a++)
		planes[a]=(UBYTE*)image + lineSize * a;
    copPtr = copSetPlanes(0, copPtr, planes, screenDepth);

   	// set colors
	for(int a=0; a < 16; a++)
		copPtr = copSetColor(copPtr, a, colors[a]);

    // terminate copper list
    *copPtr++ = 0xffff;
    *copPtr++ = 0xfffe;

    // set copper list
    custom->cop1lc = (ULONG)copper1;
    custom->dmacon = DMAF_BLITTER;//disable blitter dma for copjmp bug
	custom->copjmp1 = 0x7fff; //start coppper
}

void SetPlanes(APTR image) {
	const UBYTE* planes[screenDepth];
	for(int a=0;a<screenDepth;a++)
		planes[a]=(UBYTE*)image + lineSize * a;
	copSetPlanes(0, copperPlanes, planes, screenDepth);
}