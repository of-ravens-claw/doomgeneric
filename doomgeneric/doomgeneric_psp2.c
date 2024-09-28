#ifdef __psp2__

#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// psp2 includes
#include <kernel.h>
#include <display.h>
#include <ctrl.h>
#include <power.h>

#define ALIGN(value, align)     (((value) + (align) - 1) & ~((align) - 1))

#define KEYQUEUE_SIZE 16

static uint16_t s_KeyQueue[KEYQUEUE_SIZE];
static uint32_t s_KeyQueueWriteIndex = 0;
static uint32_t s_KeyQueueReadIndex = 0;

static void AddKeyToQueue(bool pressed, uint8_t keyCode)
{
	uint16_t keyData = (uint16_t)(pressed << 8 | keyCode);

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static void HandleKeyInput(void)
{
	// this is far from ideal, but we need to deal with inputs somehow...
	SceCtrlData data;
	memset(&data, 0, sizeof(data));

	sceCtrlPeekBufferPositive(0, &data, 1);

	AddKeyToQueue((data.buttons & SCE_CTRL_SELECT) == SCE_CTRL_SELECT, KEY_ESCAPE);
	AddKeyToQueue((data.buttons & SCE_CTRL_START) == SCE_CTRL_START, KEY_ENTER);

	AddKeyToQueue((data.buttons & SCE_CTRL_TRIANGLE) == SCE_CTRL_TRIANGLE, KEY_UPARROW);
	AddKeyToQueue((data.buttons & SCE_CTRL_SQUARE) == SCE_CTRL_SQUARE, KEY_LEFTARROW);
	AddKeyToQueue((data.buttons & SCE_CTRL_CIRCLE) == SCE_CTRL_CIRCLE, KEY_RIGHTARROW);
	AddKeyToQueue((data.buttons & SCE_CTRL_CROSS) == SCE_CTRL_CROSS, KEY_DOWNARROW);

	AddKeyToQueue((data.buttons & SCE_CTRL_R) == SCE_CTRL_R, KEY_FIRE);
	AddKeyToQueue((data.buttons & SCE_CTRL_L) == SCE_CTRL_L, KEY_USE);
}

#define SCREEN_FB_SIZE ALIGN((DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4), (256 * 1024)) // must be 256kb aligned

SceUID g_displayBlock;

void DG_Init(void)
{
	// We need the data to be 256kb aligned, but the previous malloc call won't do.
	// Thus, we're freeing the data that was previously allocated, and allocating it properly.
	//
	// We also need CDRAM (video memory), malloc gives us regular RAM.
	free(DG_ScreenBuffer);

	g_displayBlock = sceKernelAllocMemBlock("display", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, SCREEN_FB_SIZE, NULL);
	if (g_displayBlock < 0)
	{
		printf("sceKernelAllocMemBlock returned 0x%08X\n", g_displayBlock);
		abort();
	}
	sceKernelGetMemBlockBase(g_displayBlock, (void**)&DG_ScreenBuffer);

	SceDisplayFrameBuf frame = 
	{
		sizeof(SceDisplayFrameBuf),
		DG_ScreenBuffer,
		DOOMGENERIC_RESX,
		SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,
		DOOMGENERIC_RESX,
		DOOMGENERIC_RESY
	};
	sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);

	memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(uint16_t));
}

void DG_DrawFrame(void)
{
	// Do I need to do anything aside from handling input?

	HandleKeyInput();
}

void DG_SleepMs(uint32_t ms)
{
	sceKernelDelayThread(ms * 1000);
}

uint32_t DG_GetTicksMs(void)
{
	// System time is in microseconds, we want milliseconds
	return (uint32_t)(sceKernelGetSystemTimeWide() / 1000);
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
	if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
	{
		// key queue is empty
		return 0;
	}

	unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
	s_KeyQueueReadIndex++;
	s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

	*pressed = keyData >> 8;
	*doomKey = keyData & 0xFF;

	return 1;
}

void DG_SetWindowTitle(const char* title)
{
	// we don't care, but anyway.
	printf("DG_SetWindowTitle: Tried to set window title to '%s'\n", title);
}

int main(int argc, char **argv)
{
	doomgeneric_Create(argc, argv);
	scePowerSetArmClockFrequency(444);

	while (true)
	{
		doomgeneric_Tick();
	}

	// this will likely never run but alas
	sceDisplaySetFrameBuf(NULL, SCE_DISPLAY_UPDATETIMING_NEXTHSYNC);
	sceKernelFreeMemBlock(g_displayBlock);
}
#endif