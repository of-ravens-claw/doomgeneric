#ifdef __ORBIS__

#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <string.h>

#include <kernel.h>
#include <video_out.h>
#include <user_service.h>
#include <libsysmodule.h> // for ime

#define USE_GAMEPAD_AS_KEYBOARD
//#define USE_IME_KEYBOARD

#if defined(USE_GAMEPAD_AS_KEYBOARD) && defined(USE_IME_KEYBOARD)
#error "Both USE_GAMEPAD_AS_KEYBOARD and USE_IME_KEYBOARD are defined! Only one can be defined at a time!"
#endif

// If you're using a gamepad as a keyboard, the controls are as follows:
//
// R1       - Fire key
// L1       - Use key
//
// Options  - Menu Confirm key
// Touchpad - Menu Back key
//
// Triangle - Up Arrow
// Circle   - Left Arrow
// Square   - Right Arrow
// Cross    - Down Arrow

#ifdef USE_GAMEPAD_AS_KEYBOARD
#include <pad.h>
#pragma comment(lib, "ScePad_stub_weak")
#endif

#ifdef USE_IME_KEYBOARD
#include <libime.h>
#pragma comment(lib, "SceIme_stub_weak")
#endif

#define D_ALIGN(value, align)     (((value) + (align) - 1) & ~((align) - 1))

#define KEYQUEUE_SIZE 16

static uint16_t s_KeyQueue[KEYQUEUE_SIZE];
static uint32_t s_KeyQueueWriteIndex = 0;
static uint32_t s_KeyQueueReadIndex = 0;

// can't assert normally
#ifdef _DEBUG
#undef assert
#define assert(x)	if (!(x)) { printf("%s (%d) %s: Assertion Failed (%s)\n", __FILE__, __LINE__, __func__, #x); }
#define dprintf printf
#else
#undef assert
#define assert(x)
#define dprintf(...)
#endif

#ifdef USE_GAMEPAD_AS_KEYBOARD
static uint8_t ConvertToDoomKey(uint8_t key)
{
	return key;
}
#endif

#ifdef USE_IME_KEYBOARD
static uint8_t ConvertToDoomKey(uint8_t key)
{
	switch (key)
	{
	case SCE_IME_KEYCODE_RETURN:
		key = KEY_ENTER;
		break;
	case SCE_IME_KEYCODE_ESCAPE:
		key = KEY_ESCAPE;
		break;

	case SCE_IME_KEYCODE_LEFTARROW:
		key = KEY_LEFTARROW;
		break;
	case SCE_IME_KEYCODE_RIGHTARROW:
		key = KEY_RIGHTARROW;
		break;
	case SCE_IME_KEYCODE_UPARROW:
		key = KEY_UPARROW;
		break;
	case SCE_IME_KEYCODE_DOWNARROW:
		key = KEY_DOWNARROW;
		break;

	case SCE_IME_KEYCODE_SPACEBAR:
		key = KEY_USE;
		break;

	case SCE_IME_KEYCODE_EQUAL:
		key = KEY_EQUALS;
		break;
	case SCE_IME_KEYCODE_MINUS:
		key = KEY_MINUS;
		break;

	case SCE_IME_KEYCODE_LEFTCONTROL:
	case SCE_IME_KEYCODE_RIGHTCONTROL:
		key = KEY_FIRE;
		break;

	case SCE_IME_KEYCODE_LEFTSHIFT:
	case SCE_IME_KEYCODE_RIGHTSHIFT:
		key = KEY_RSHIFT;
		break;

	case SCE_IME_KEYCODE_LEFTALT:
	case SCE_IME_KEYCODE_RIGHTALT:
		key = KEY_RALT;
		break;

	case SCE_IME_KEYCODE_COMMA:
		key = KEY_STRAFE_L;
		break;
	case SCE_IME_KEYCODE_PERIOD:
		key = KEY_STRAFE_R;
		break;

	case SCE_IME_KEYCODE_TAB:
		key = KEY_TAB;
		break;

	case SCE_IME_KEYCODE_BACKSPACE:
		key = KEY_BACKSPACE;
		break;

	// remap from ime keycodes to lowercase ascii
	default:
		if (key >= SCE_IME_KEYCODE_A && key <= SCE_IME_KEYCODE_Z)
			key = key + 'a' - SCE_IME_KEYCODE_A; // letters

		// special case for 0, since ascii 0 comes first but ime 0 comes last
		if (key == SCE_IME_KEYCODE_0)
			key = '0';

		if (key >= SCE_IME_KEYCODE_1 && key <= SCE_IME_KEYCODE_9)
			key = key + '1' - SCE_IME_KEYCODE_1;

		break;
	}

	return key;
}

static void AddKeyToQueue(bool pressed, uint8_t keyCode); // fwd decl
void HandleIMEKeyboardEvent(void* arg, const SceImeEvent* e)
{
	switch (e->id)
	{
	case SCE_IME_KEYBOARD_EVENT_CONNECTION:
		printf("Keyboard is connected[uid=0x%08x]:Resource ID(%d, %d, %d)\n", e->param.resourceIdArray.userId, e->param.resourceIdArray.resourceId[0], e->param.resourceIdArray.resourceId[1], e->param.resourceIdArray.resourceId[2]);
		break;
	case SCE_IME_KEYBOARD_EVENT_DISCONNECTION:
		printf("Keyboard is disconnected[uid=0x%08x]:Resource ID(%d, %d, %d)\n", e->param.resourceIdArray.userId, e->param.resourceIdArray.resourceId[0], e->param.resourceIdArray.resourceId[1], e->param.resourceIdArray.resourceId[2]);
		break;
	case SCE_IME_KEYBOARD_EVENT_ABORT:
		printf("Keyboard manager was aborted\n");
		break;
	case SCE_IME_KEYBOARD_EVENT_OPEN:
		// Processing when keyboard reception is started
		// Do not clear state as we only get events when state changes.
		break;

	case SCE_IME_KEYBOARD_EVENT_KEYCODE_DOWN: // fallthrough
	case SCE_IME_KEYBOARD_EVENT_KEYCODE_UP:
	{
		const SceImeKeycode* data = &e->param.keycode;
		bool isDown = e->id == SCE_IME_KEYBOARD_EVENT_KEYCODE_DOWN;
		AddKeyToQueue(isDown, data->keycode);
	}
	break;

	default: break;
	}

}
#endif

static void AddKeyToQueue(bool pressed, uint8_t keyCode)
{
	// printf("[AddKeyToQueue]: Key 0x%02X, press: %d\n", keyCode, pressed);
	uint8_t key = ConvertToDoomKey(keyCode);
	uint16_t keyData = (uint16_t)(pressed << 8 | key);

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static int g_inputHandle;
static int g_videoOutHandle;

#define SCREEN_FB_SIZE D_ALIGN((DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4), (64 * 1024)) // 64kib alignment needed

void DG_Init(void)
{
	int g_initialUser;
	int ret;

	// See PSP2 code for explanations
	// Except we need it to be 64kb aligned here, and in the Garlic bus instead of CDRAM.
	// But functionally Garlic memory is the same as CDRAM, so.
	free(DG_ScreenBuffer);
	DG_ScreenBuffer = NULL; // needed otherwise sceKernelMapDirectMemory will not work properly.

	ret = sceUserServiceInitialize(NULL);
	assert(ret == SCE_OK);

	ret = sceUserServiceGetInitialUser(&g_initialUser);
	assert(ret == SCE_OK);

#ifdef USE_GAMEPAD_AS_KEYBOARD
	ret = scePadInit();
	assert(ret == SCE_OK);

	ret = scePadOpen(g_initialUser, SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);
	assert(ret >= 0);
	g_inputHandle = ret;
#endif

#ifdef USE_IME_KEYBOARD
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_LIBIME);
	assert(ret == SCE_SYSMODULE_LOADED);

	SceImeKeyboardParam kparam = {};
	kparam.option = SCE_IME_KEYBOARD_OPTION_DEFAULT;
	kparam.handler = HandleIMEKeyboardEvent;

	ret = sceImeKeyboardOpen(g_initialUser, &kparam);
	assert(ret >= 0);
	g_inputHandle = ret; // not actually used, but the compiler complains, so.
#endif

	// setup VideoOut and acquire handle
	ret = sceVideoOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
	assert((ret >= 0));
	g_videoOutHandle = ret;

	// setup direct memory for display buffers
	const uint32_t shaderGpuAlignment = 64 * 1024;

	off_t offsetOut;
	ret = sceKernelAllocateDirectMemory(
		0,
		SCE_KERNEL_MAIN_DMEM_SIZE,
		SCREEN_FB_SIZE,
		shaderGpuAlignment,
		SCE_KERNEL_WC_GARLIC,
		&offsetOut);
	assert((ret == 0));

	ret = sceKernelMapDirectMemory(
		(void**)&DG_ScreenBuffer,
		SCREEN_FB_SIZE,
		SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_READ,
		0,
		offsetOut,
		shaderGpuAlignment);
	assert((ret >= 0));
	dprintf("sceKernelMapDirectMemory - ret 0x%08X\n", ret);

	// register display buffers
	SceVideoOutBufferAttribute attribute;

	sceVideoOutSetBufferAttribute(&attribute,
		SCE_VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB,
		SCE_VIDEO_OUT_TILING_MODE_LINEAR,
		SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
		DOOMGENERIC_RESX, 
		DOOMGENERIC_RESY, 
		D_ALIGN(DOOMGENERIC_RESX, 64));

	ret = sceVideoOutRegisterBuffers(g_videoOutHandle, 0, (void**)&DG_ScreenBuffer, 1, &attribute);
	assert((ret >= 0));
	dprintf("sceVideoOutRegisterBuffers - ret 0x%08X\n", ret);

	// set flip conditions
	ret = sceVideoOutSetFlipRate(g_videoOutHandle, 0);
	assert(ret >= 0);

	memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(uint16_t));
}

static void HandleKeyInput(void)
{
	int ret;

#ifdef USE_GAMEPAD_AS_KEYBOARD
	// this is far from ideal, but we need to deal with inputs somewhere...
	ScePadData pad = {};
	ret = scePadReadState(g_inputHandle, &pad);
	assert(ret == SCE_OK);

	AddKeyToQueue((pad.buttons & SCE_PAD_BUTTON_OPTIONS) == SCE_PAD_BUTTON_OPTIONS, KEY_ENTER);
	AddKeyToQueue((pad.buttons & SCE_PAD_BUTTON_TOUCH_PAD) == SCE_PAD_BUTTON_TOUCH_PAD, KEY_ESCAPE);

	AddKeyToQueue((pad.buttons & SCE_PAD_BUTTON_TRIANGLE) == SCE_PAD_BUTTON_TRIANGLE, KEY_UPARROW);
	AddKeyToQueue((pad.buttons & SCE_PAD_BUTTON_SQUARE) == SCE_PAD_BUTTON_SQUARE, KEY_LEFTARROW);
	AddKeyToQueue((pad.buttons & SCE_PAD_BUTTON_CIRCLE) == SCE_PAD_BUTTON_CIRCLE, KEY_RIGHTARROW);
	AddKeyToQueue((pad.buttons & SCE_PAD_BUTTON_CROSS) == SCE_PAD_BUTTON_CROSS, KEY_DOWNARROW);

	AddKeyToQueue((pad.buttons & SCE_PAD_BUTTON_R1) == SCE_PAD_BUTTON_R1, KEY_FIRE);
	AddKeyToQueue((pad.buttons & SCE_PAD_BUTTON_L1) == SCE_PAD_BUTTON_L1, KEY_USE);
#endif

#ifdef USE_IME_KEYBOARD
	ret = sceImeUpdate(HandleIMEKeyboardEvent);
	assert(ret >= 0);
#endif
}

void DG_DrawFrame(void)
{
	int ret;

	// request flip to the buffer - I don't know why this must be done manually, but alas.
	ret = sceVideoOutSubmitFlip(g_videoOutHandle, 0, SCE_VIDEO_OUT_FLIP_MODE_VSYNC, 0);
	assert(ret >= 0);
	dprintf("sceVideoOutSubmitFlip - ret 0x%08X\n", ret);

	HandleKeyInput();
}

void DG_SleepMs(uint32_t ms)
{
	sceKernelUsleep(ms * 1000);
}

uint32_t DG_GetTicksMs(void)
{
	return (sceKernelGetProcessTimeCounter() * 1000) / sceKernelGetProcessTimeCounterFrequency();
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

	// We should probably clean up here, but since it'll never run, we can ignore it.
}
#endif // __ORBIS__