#ifdef __psp__
// Hacked together from the PSP2 port. Not expected to work very well.

#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// psp includes
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspge.h>

#ifndef SCE_DISPLAY_MODE_LCD
#  define SCE_DISPLAY_MODE_LCD 0 /* the homebrew SDK is missing this... */
#endif

/* Define the module info section */
PSP_MODULE_INFO("DOOMGENERIC", 0, 1, 1);

/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

#define KEYQUEUE_SIZE 16

static uint16_t s_KeyQueue[KEYQUEUE_SIZE];
static uint32_t s_KeyQueueWriteIndex = 0;
static uint32_t s_KeyQueueReadIndex = 0;

static void AddKeyToQueue(bool pressed, uint8_t keyCode)
{
	s_KeyQueue[s_KeyQueueWriteIndex] = (uint16_t)(pressed << 8 | keyCode);
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static void HandleKeyInput(void)
{
	// this is far from ideal, but we need to deal with inputs somehow...
	SceCtrlData data;
	memset(&data, 0, sizeof(data));

	sceCtrlPeekBufferPositive(&data, 1);

	AddKeyToQueue((data.Buttons & PSP_CTRL_SELECT) == PSP_CTRL_SELECT, KEY_ESCAPE);
	AddKeyToQueue((data.Buttons & PSP_CTRL_START) == PSP_CTRL_START, KEY_ENTER);

	AddKeyToQueue((data.Buttons & PSP_CTRL_TRIANGLE) == PSP_CTRL_TRIANGLE, KEY_UPARROW);
	AddKeyToQueue((data.Buttons & PSP_CTRL_SQUARE) == PSP_CTRL_SQUARE, KEY_LEFTARROW);
	AddKeyToQueue((data.Buttons & PSP_CTRL_CIRCLE) == PSP_CTRL_CIRCLE, KEY_RIGHTARROW);
	AddKeyToQueue((data.Buttons & PSP_CTRL_CROSS) == PSP_CTRL_CROSS, KEY_DOWNARROW);

	AddKeyToQueue((data.Buttons & PSP_CTRL_RTRIGGER) == PSP_CTRL_RTRIGGER, KEY_FIRE);
	AddKeyToQueue((data.Buttons & PSP_CTRL_LTRIGGER) == PSP_CTRL_LTRIGGER, KEY_USE);
}

void I_Error (char *error, ...);

static int exit_func(int count, int arg1, void *arg2)
{
	(void)count;
	(void)arg1;
	(void)arg2;

	/*J ゲーム終了システムサービスを呼び出す */
	/*E Call game ending system service */
	sceKernelExitGame();

	/*J 上記関数はこの関数内に戻ってこない */
	/*E The above function does not return within this function  */
	return 0;
}

void DG_Init(void)
{
	free(DG_ScreenBuffer);

	/*J 「HOME」ボタンによって受け取るコールバックの作成 */
	/*E Create callback for receiving according to HOME button  */
	SceUID exit_cb = sceKernelCreateCallback("ExitGame", exit_func, NULL);
	if (exit_cb <= 0)
	{
		I_Error("fatal error: sceKernelCreateCallback() 0x%x\n", exit_cb);
		return;
	}
	/*J 終了コールバックの登録 */
	/*E Register the termination callback */
	sceKernelRegisterExitCallback(exit_cb);

	DG_ScreenBuffer = sceGeEdramGetAddr();
	sceDisplaySetMode(SCE_DISPLAY_MODE_LCD, 480, 272);
	sceDisplaySetFrameBuf(DG_ScreenBuffer, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);

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
	return sceKernelGetSystemTimeWide() / 1000;
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
	if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
	{
		// key queue is empty
		return 0;
	}

	uint16_t keyData = s_KeyQueue[s_KeyQueueReadIndex];
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

	while (1)
	{
		doomgeneric_Tick();
	}
}
#endif