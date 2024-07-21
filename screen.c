#include "support/gcc8_c_support.h"
#include <exec/execbase.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include "hardware.h"


// set up a 320x256 lowres display
static __attribute__((always_inline)) inline USHORT* screenScanDefault(USHORT* copListEnd, 
USHORT width, USHORT height) {

	// width != 320 is untested

	const USHORT x=0x81+8;
	const USHORT y=0x2c;
	const USHORT RES=8; //8=lowres,4=hires
	USHORT xstop = x+width-8;
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

static __attribute__((always_inline)) inline USHORT* copSetPlanes(UBYTE bplPtrStart,USHORT* copListEnd,const UBYTE **planes,int numPlanes) {
	for (USHORT i=0;i<numPlanes;i++) {
		ULONG addr=(ULONG)planes[i];
		*copListEnd++=offsetof(struct Custom, bplpt[0]) + (i + bplPtrStart) * sizeof(APTR);
		*copListEnd++=(UWORD)(addr>>16);
		*copListEnd++=offsetof(struct Custom, bplpt[0]) + (i + bplPtrStart) * sizeof(APTR) + 2;
		*copListEnd++=(UWORD)addr;
	}
	return copListEnd;
}


static __attribute__((always_inline)) inline USHORT* copSetColor(USHORT* copListCurrent,USHORT index,USHORT color) {
	*copListCurrent++=offsetof(struct Custom, color) + sizeof(UWORD) * index;
	*copListCurrent++=color;
	return copListCurrent;
}

// Color rotation 6>12>3>7>15>14>13>10>4>8>0
// 2>4
// 9>3
// 5>11>7
static const UWORD colors[] = {
    0x000, // 0
    0x53e, // 7
    0x43f, // 6
	0x60a, // 13
    0x52e, // 8
    0x42d, // 5
    0x218, // 3
    0x60a, // 12
    0x103, // 1
    0x52e, // 8
    0x52d, // 9
    0x32a, // 4
    0x115, // 2
    0x51c, // 10
    0x61b, // 11
    0x60a, // 13
};

static const UWORD colors1[] = {
    0x000, // 0
    0x999, // 7
    0x888, // 6
	0xfff, // 13
    0xaaa, // 8
    0x777, // 5
    0x555, // 3
    0xeee, // 12
    0x222, // 1
    0x999, // 8
    0xbbb, // 9
    0x666, // 4
    0x444, // 2
    0xccc, // 10
    0xddd, // 11
    0xfff, // 13
};

static USHORT* copper1 = NULL;
static UWORD screenDepth = 4;
static UWORD *copperPlanes;
static const UWORD lineSize = 320/8;


// width ignored
void SetupScreenComplete(APTR image, UWORD depth, UWORD width, UWORD height) {
	screenDepth = depth;
   	copper1 = (USHORT*)AllocMem(1024, MEMF_CHIP);
	USHORT* copPtr = copper1;

  	// register graphics resources with WinUAE for nicer gfx debugger experience
	debug_register_copperlist(copper1, "copper1", 1024, 0);

	copPtr = screenScanDefault(copPtr, 320, height);
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
	custom->copjmp1 = 0x7fff; //start coppper
	KPrintF("Copper list length: %ld", (ULONG)(copPtr-copper1));
}

void SetupScreen(APTR image, UWORD depth) {
	SetupScreenComplete(image, depth, 0, 160);
}


void CleanupScreen() {
	FreeMem(copper1, 1024);
}

void SetPlanes(APTR image) {
	const UBYTE* planes[screenDepth];
	for(int a=0;a<screenDepth;a++)
		planes[a]=(UBYTE*)image + lineSize * a;
	copSetPlanes(0, copperPlanes, planes, screenDepth);
}
