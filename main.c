#include "support/gcc8_c_support.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <exec/execbase.h>
#include <graphics/gfxmacros.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>
#include "effect_sub.h"
#include "effect_add.h"
#include "hardware.h"
#include "screen.h"
//config

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;

//backup
static UWORD SystemInts;
static UWORD SystemDMA;
static UWORD SystemADKCON;
static volatile APTR VBR=0;
static APTR SystemIrq;
 
struct View *ActiView;

static APTR GetVBR(void) {
	APTR vbr = 0;
	UWORD getvbr[] = { 0x4e7a, 0x0801, 0x4e73 }; // MOVEC.L VBR,D0 RTE

	if (SysBase->AttnFlags & AFF_68010) 
		vbr = (APTR)Supervisor((ULONG (*)())getvbr);

	return vbr;
}

void SetInterruptHandler(APTR interrupt) {
	*(volatile APTR*)(((UBYTE*)VBR)+0x6c) = interrupt;
}

APTR GetInterruptHandler() {
	return *(volatile APTR*)(((UBYTE*)VBR)+0x6c);
}

//vblank begins at vpos 312 hpos 1 and ends at vpos 25 hpos 1
//vsync begins at line 2 hpos 132 and ends at vpos 5 hpos 18 
void WaitVbl() {
	debug_start_idle();
	while (1) {
		volatile ULONG vpos=*(volatile ULONG*)0xDFF004;
		vpos&=0x1ff00;
		if (vpos!=(311<<8))
			break;
	}
	while (1) {
		volatile ULONG vpos=*(volatile ULONG*)0xDFF004;
		vpos&=0x1ff00;
		if (vpos==(311<<8))
			break;
	}
	debug_stop_idle();
}

void WaitLine(USHORT line) {
	while (1) {
		volatile ULONG vpos=*(volatile ULONG*)0xDFF004;
		if(((vpos >> 8) & 511) == line)
			break;
	}
}

__attribute__((always_inline)) inline void WaitBlt() {
	UWORD tst=*(volatile UWORD*)&custom->dmaconr; //for compatiblity a1000
	(void)tst;
	while (*(volatile UWORD*)&custom->dmaconr&(1<<14)) {} //blitter busy wait
}

void TakeSystem() {
	Forbid();
	//Save current interrupts and DMA settings so we can restore them upon exit. 
	SystemADKCON=custom->adkconr;
	SystemInts=custom->intenar;
	SystemDMA=custom->dmaconr;
	ActiView=GfxBase->ActiView; //store current view

	LoadView(0);
	WaitTOF();
	WaitTOF();

	WaitVbl();
	WaitVbl();

	OwnBlitter();
	WaitBlit();	
	Disable();
	
	custom->intena=0x7fff;//disable all interrupts
	custom->intreq=0x7fff;//Clear any interrupts that were pending
	
	custom->dmacon=0x7fff;//Clear all DMA channels

	//set all colors black
	for(int a=0;a<32;a++)
		custom->color[a]=0;

	WaitVbl();
	WaitVbl();

	VBR=GetVBR();
	SystemIrq=GetInterruptHandler(); //store interrupt register
}

void FreeSystem() { 
	WaitVbl();
	WaitBlit();
	custom->intena=0x7fff;//disable all interrupts
	custom->intreq=0x7fff;//Clear any interrupts that were pending
	custom->dmacon=0x7fff;//Clear all DMA channels

	//restore interrupts
	SetInterruptHandler(SystemIrq);

	/*Restore system copper list(s). */
	custom->cop1lc=(ULONG)GfxBase->copinit;
	custom->cop2lc=(ULONG)GfxBase->LOFlist;
	custom->copjmp1=0x7fff; //start coppper

	/*Restore all interrupts and DMA settings. */
	custom->intena=SystemInts|0x8000;
	custom->dmacon=SystemDMA|0x8000;
	custom->adkcon=SystemADKCON|0x8000;

	WaitBlit();	
	DisownBlitter();
	Enable();

	LoadView(ActiView);
	WaitTOF();
	WaitTOF();

	Permit();
}

__attribute__((always_inline)) inline short MouseLeft(){return !((*(volatile UBYTE*)0xbfe001)&64);}	
__attribute__((always_inline)) inline short MouseRight(){return !((*(volatile UWORD*)0xdff016)&(1<<10));}

volatile short frameCounter = 0;


__attribute__((always_inline)) inline USHORT* copWaitXY(USHORT *copListEnd,USHORT x,USHORT i) {
	*copListEnd++=(i<<8)|(x<<1)|1;	//bit 1 means wait. waits for vertical position x<<8, first raster stop position outside the left 
	*copListEnd++=0xfffe;
	return copListEnd;
}

__attribute__((always_inline)) inline USHORT* copWaitY(USHORT* copListEnd,USHORT i) {
	*copListEnd++=(i<<8)|4|1;	//bit 1 means wait. waits for vertical position x<<8, first raster stop position outside the left 
	*copListEnd++=0xfffe;
	return copListEnd;
}

UWORD* scroll = NULL;

static const UBYTE sinus15[] = { 
	8,8,9,10,10,11,12,12,
	13,13,14,14,14,15,15,15,
	15,15,15,15,14,14,14,13,
	13,12,12,11,10,10,9,8,
	8,7,6,5,5,4,3,3,
	2,2,1,1,1,0,0,0,
	0,0,0,0,1,1,1,2,
	2,3,3,4,5,5,6,7, 
};


static const UBYTE sinus32[] = {
	16,18,20,22,24,25,27,28,
	30,30,31,32,32,32,32,31,
	30,30,28,27,25,24,22,20,
	18,16,14,12,10,8,7,5,
	4,2,2,1,0,0,0,0,
	1,2,2,4,5,7,8,10,
	12,14,16,
};

static __attribute__((interrupt)) void interruptHandler() {
	custom->intreq=(1<<INTB_VERTB); custom->intreq=(1<<INTB_VERTB); //reset vbl req. twice for a4000 bug.
	// DEMO - increment frameCounter
	frameCounter++;
}


static void Wait10() { WaitLine(0x10); }
static void WaitBOF() { WaitLine(254); }


typedef struct DemoEffect{
	void (* initialize)();
	void (* cleanup)();
	BOOL (* effect)(BOOL);
} DemoEffect;

static const UWORD effectCount = 2;
static DemoEffect effects[] = {
	{
		.initialize = Sub_InitEffect,
		.cleanup = Sub_FreeEffect,
		.effect = Sub_CalcEffect
	},
	{
		.initialize = Add_InitEffect,
		.cleanup = Add_FreeEffect,
		.effect = Add_CalcEffect
	}
};


int main() {
	SysBase = *((struct ExecBase**)4UL);

	// We will use the graphics library only to locate and restore the system copper list once we are through.
	GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library",0);
	if (!GfxBase)
		Exit(0);

	// used for printing
	DOSBase = (struct DosLibrary*)OpenLibrary((CONST_STRPTR)"dos.library", 0);
	if (!DOSBase)
		Exit(0);

	TakeSystem();
	WaitVbl();
	
	custom->dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_RASTER | DMAF_COPPER | DMAF_BLITTER;
	custom->dmacon = DMAF_MASTER | DMAF_RASTER | DMAF_COPPER | DMAF_BLITTER;
	// DEMO
	UWORD currentEffect = 0;	
	effects[currentEffect].initialize();

	custom->dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_RASTER | DMAF_COPPER | DMAF_BLITTER;
	SetInterruptHandler((APTR)interruptHandler);
	custom->intena = INTF_SETCLR | INTF_INTEN | INTF_VERTB;
	custom->intreq=(1<<INTB_VERTB);//reset vbl req
	
	while(!MouseRight()) {
		WaitBOF();		
		if ( MouseLeft() ) {
			currentEffect++;
			if ( currentEffect >= effectCount ) {
				break;
			}
			effects[currentEffect-1].cleanup();
			custom->dmacon = DMAF_MASTER | DMAF_RASTER | DMAF_COPPER | DMAF_BLITTER;
			effects[currentEffect].initialize();
			custom->dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_RASTER | DMAF_COPPER | DMAF_BLITTER;
		}
		effects[currentEffect].effect(TRUE);		
		//Sub_CalcEffect(TRUE);
	}
	effects[currentEffect-1].cleanup();


	// END
	FreeSystem();

	CloseLibrary((struct Library*)DOSBase);
	CloseLibrary((struct Library*)GfxBase);
}
