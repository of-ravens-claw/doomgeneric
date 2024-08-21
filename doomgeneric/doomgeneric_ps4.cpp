#ifdef __ORBIS__

#include "doomkeys.h"

#include "doomgeneric.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <assert.h>
#include <unistd.h>

#include <kernel.h>
#include <rtc.h>
#include <video_out.h>
#include <user_service.h>
#include <libsysmodule.h>
#include <gnm.h>

//#define USE_PHYSICAL_KEYBOARD
#define USE_IME_KEYBOARD // still a physical keyboard, but a different library.
//#define USE_GAMEPAD_AS_KEYBOARD

// If you're using a gamepad as a keyboard, the controls are as follows:
// D-PAD    - Arrow keys
// R1       - Right Ctrl
// L1       - Space
// Circle   - Escape
// Cross    - Enter
// Square   - Right Shift
// Triangle - Y (Needed for the difficulty prompt, ugh)

#ifdef USE_GAMEPAD_AS_KEYBOARD
#include <pad.h>
#pragma comment(lib, "ScePad_stub_weak")
#endif

#ifdef USE_PHYSICAL_KEYBOARD
#include <dbg_keyboard.h>
#pragma comment(lib, "SceDbgKeyboard_stub_weak")
#endif

#ifdef USE_IME_KEYBOARD
#include <libime.h>
#pragma comment( lib , "SceIme_stub_weak" )
#endif

#pragma comment(lib, "SceSysModule_stub_weak")
#pragma comment(lib, "SceUserService_stub_weak")

#define KEYQUEUE_SIZE (16) // temporary

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

#ifdef USE_GAMEPAD_AS_KEYBOARD
static unsigned char ConvertToDoomKey(unsigned int _key)
{
	unsigned char key = 0;
	switch (_key)
	{
	case SCE_PAD_BUTTON_CROSS:
		key = KEY_ENTER;
		break;
	case SCE_PAD_BUTTON_CIRCLE:
		key = KEY_ESCAPE;
		break;
	case SCE_PAD_BUTTON_SQUARE:
		key = KEY_RSHIFT;
		break;
	case SCE_PAD_BUTTON_TRIANGLE:
		key = 'y';
		break;

	case SCE_PAD_BUTTON_LEFT:
		key = KEY_LEFTARROW;
		break;
	case SCE_PAD_BUTTON_RIGHT:
		key = KEY_RIGHTARROW;
		break;
	case SCE_PAD_BUTTON_UP:
		key = KEY_UPARROW;
		break;
	case SCE_PAD_BUTTON_DOWN:
		key = KEY_DOWNARROW;
		break;

	case SCE_PAD_BUTTON_R1:
		key = KEY_FIRE;
		break;
	case SCE_PAD_BUTTON_L1:
		key = KEY_USE;
		break;

	default: break;
	}

	return key;
}
#endif

#ifdef USE_PHYSICAL_KEYBOARD
// Custom codes, because the PS4 is weird when it comes to modifier keys.
#define SCE_DBG_KEYBOARD_CODE_L_CTRL  (0xF0)
#define SCE_DBG_KEYBOARD_CODE_R_CTRL  (0xF1)
#define SCE_DBG_KEYBOARD_CODE_L_SHIFT (0xF2)
#define SCE_DBG_KEYBOARD_CODE_R_SHIFT (0xF3)

static unsigned char ConvertToDoomKey(unsigned int _key)
{
	unsigned char key = 0;
	switch (_key)
	{
	case SCE_DBG_KEYBOARD_CODE_ENTER:
		key = KEY_ENTER;
		break;
	case SCE_DBG_KEYBOARD_CODE_ESC:
		key = KEY_ESCAPE;
		break;
	case SCE_DBG_KEYBOARD_CODE_LEFT_ARROW:
		key = KEY_LEFTARROW;
		break;
	case SCE_DBG_KEYBOARD_CODE_RIGHT_ARROW:
		key = KEY_RIGHTARROW;
		break;
	case SCE_DBG_KEYBOARD_CODE_UP_ARROW:
		key = KEY_UPARROW;
		break;
	case SCE_DBG_KEYBOARD_CODE_DOWN_ARROW:
		key = KEY_DOWNARROW;
		break;
	case SCE_DBG_KEYBOARD_CODE_L_CTRL:
	case SCE_DBG_KEYBOARD_CODE_R_CTRL:
		key = KEY_FIRE;
		break;
	case SCE_DBG_KEYBOARD_CODE_SPACE:
		key = KEY_USE;
		break;
	case SCE_DBG_KEYBOARD_CODE_L_SHIFT:
	case SCE_DBG_KEYBOARD_CODE_R_SHIFT:
		key = KEY_RSHIFT;
		break;
	case SCE_DBG_KEYBOARD_CODE_Y:
		key = 'y';
		break;

	// oh fuck.
	default:
		key = _key;
		break;
	}

	return key;
}
#endif

#ifdef USE_IME_KEYBOARD
static unsigned char ConvertToDoomKey(unsigned int _key)
{
	unsigned char key = 0;
	switch (_key)
	{
	case 0x28:
		key = KEY_ENTER;
		break;
	case 0x29:
		key = KEY_ESCAPE;
		break;
	case 0x50:
		key = KEY_LEFTARROW;
		break;
	case 0x4F:
		key = KEY_RIGHTARROW;
		break;
	case 0x52:
		key = KEY_UPARROW;
		break;
	case 0x51:
		key = KEY_DOWNARROW;
		break;
	case 0x2C:
		key = KEY_USE;
		break;
	case 0x1C:
		key = 'y';
		break;

	// these are a HACK for fpPS4!
	case 0x5C:
		key = KEY_LEFTARROW;
		break;
	case 0x5E:
		key = KEY_RIGHTARROW;
		break;
	case 0x60:
		key = KEY_UPARROW;
		break;
	case 0x5A:
		key = KEY_DOWNARROW;
		break;

	default:
		key = _key;
		break;
	}

	return key;
}
#endif

static void AddKeyToQueue(int pressed, unsigned int keyCode)
{
	printf("[AddKeyToQueue]: Key 0x%02X, press: %d\n", keyCode, pressed);

	unsigned char key = ConvertToDoomKey(keyCode);

	unsigned short keyData = (pressed << 8) | key;

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

#pragma region copied from libSceVideoOut samples

#define DISPLAY_BUFFER_NUM 2 // minimum 2 required
#define FLIP_RATE 0 // 0: no fliprate is set. 1: 30fps on 60Hz video output  2: 20fps
#define FLIP_MODE SCE_VIDEO_OUT_FLIP_MODE_VSYNC // SceVideoOutFlipMode

#define VIDEO_MEMORY_STACK_SIZE (1024 * 1024 * 192)

using namespace sce;

// display buffer attributes
class RenderBufferAttributes
{
public:
	void init(int width, int height = 0, bool isTile = true)
	{
		// set basic parameters
		m_width = width;
		if (height == 0) m_height = m_width * 9 / 16;
		else m_height = height;
		m_pixelFormat = SCE_VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB;
		m_dataFormat = Gnm::kDataFormatB8G8R8A8UnormSrgb;
		m_bpp = sizeof(uint32_t);
		m_isTile = isTile;

		// set other parameters
		Gnm::RenderTargetSpec rtSpec;
		rtSpec.init();
		rtSpec.m_width = m_width;
		rtSpec.m_height = m_height;
		rtSpec.m_pitch = 0;
		rtSpec.m_numSlices = 1;
		rtSpec.m_colorFormat = m_dataFormat;
		rtSpec.m_numSamples = Gnm::kNumSamples1;
		rtSpec.m_numFragments = Gnm::kNumFragments1;
		if (m_isTile) 
		{
			rtSpec.m_colorTileModeHint = Gnm::kTileModeDisplay_2dThin;
			m_tilingMode = SCE_VIDEO_OUT_TILING_MODE_TILE;
		}
		else 
		{
			rtSpec.m_colorTileModeHint = Gnm::kTileModeDisplay_LinearAligned;
			m_tilingMode = SCE_VIDEO_OUT_TILING_MODE_LINEAR;
		}

		Gnm::RenderTarget rt;
		rt.init(&rtSpec);

		m_sizeAlign = rt.getColorSizeAlign();
		m_pitch = rt.getPitch();
		int arraySlice = 0;
		m_tilingParameters.initFromRenderTarget(&rt, arraySlice);
		if (m_isTile)
		{
			m_tiler2d.init(&m_tilingParameters);
		}
		else 
		{
			m_tilerLinear.init(&m_tilingParameters);
		}
	}

	int getWidth() { return m_width; }
	int getHeight() { return m_height; }
	int getPitch() { return m_pitch; }
	int getSize() { return (int)m_sizeAlign.m_size; }
	int getAlign() { return (int)m_sizeAlign.m_align; }
	bool isTile() { return m_isTile; }

	void setDisplayBufferAttributes(SceVideoOutBufferAttribute* attribute)
	{
		sceVideoOutSetBufferAttribute(attribute,
			m_pixelFormat,
			m_tilingMode,
			SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
			m_width,
			m_height,
			m_pitch);
	}

	Gnm::SizeAlign getSizeAlign(void) { return m_sizeAlign; }

	uint64_t getTiledByteOffset(uint32_t x, uint32_t y)
	{
		uint64_t tiledByteOffset;
		uint32_t z = 0;
		int ret = 0;
		int fragmentIndex = 0;
		if (m_isTile) 
		{
			ret = m_tiler2d.getTiledElementByteOffset(&tiledByteOffset, x, y, z, fragmentIndex);
		}
		else 
		{
			ret = m_tilerLinear.getTiledElementByteOffset(&tiledByteOffset, x, y, z);
		}
		assert((ret == GpuAddress::kStatusSuccess));
		return tiledByteOffset;
	}

private:
	int m_width;
	int m_pitch;
	int m_height;
	int m_pixelFormat;
	int m_bpp;
	bool m_isTile;
	int m_tilingMode;
	Gnm::DataFormat m_dataFormat;
	GpuAddress::TilingParameters m_tilingParameters;
	Gnm::SizeAlign m_sizeAlign;
	GpuAddress::Tiler2d m_tiler2d;
	GpuAddress::TilerLinear m_tilerLinear;
};

class RenderBuffer
{
public:
	void init(RenderBufferAttributes* attr, void* address = NULL, int displayBufferIndex = -1)
	{
		m_attr = attr;
		m_address = address;
		m_displayBufferIndex = displayBufferIndex;
		m_flipArgLog = SCE_VIDEO_OUT_BUFFER_INITIAL_FLIP_ARG - 1;
	}

	void setDisplayBufferIndex(int index) { m_displayBufferIndex = index; }
	int getDisplayBufferIndex(void) { return m_displayBufferIndex; }

	void* getAddress(void) { return m_address; }

	int getLastFlipArg(void) { return m_flipArgLog; }
	void setFlipArg(int64_t flipArg) { m_flipArgLog = flipArg; }

	RenderBufferAttributes* getRenderBufferAttributes(void) { return m_attr; }

	bool isTile(void) { return m_attr->isTile(); }

	int getWidth() { return m_attr->getWidth(); }

	int getHeight() { return m_attr->getHeight(); }

	int getPitch() { return m_attr->getPitch(); }

	uint64_t getTiledByteOffset(uint32_t x, uint32_t y) { return m_attr->getTiledByteOffset(x, y); }

private:
	RenderBufferAttributes* m_attr;
	void* m_address;
	int m_displayBufferIndex; // registered index by sceVideoOutRegisterBuffers
	int64_t m_flipArgLog; // flipArg of used last time when flipping to this buffer to track where in the flip queue this buffer currently is
};

/*
 *  simple memory allocation functions for video direct memory
 */
class VideoMemory
{
public:
	void init(size_t size)
	{
		int ret;
		off_t offsetOut;

		m_stackPointer = 0;
		m_stackSize = size;
		m_stackBase = 0;


		const uint32_t  shaderGpuAlignment = 2 * 1024 * 1024;

		ret = sceKernelAllocateDirectMemory(
			0,
			SCE_KERNEL_MAIN_DMEM_SIZE,
			m_stackSize,
			shaderGpuAlignment,
			SCE_KERNEL_WC_GARLIC,
			&offsetOut);
		if (ret) {
			assert((ret == 0));
		}

		m_baseOffset = offsetOut;

		void* ptr = NULL;
		ret = sceKernelMapDirectMemory(
			&ptr,
			m_stackSize,
			SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_READ | SCE_KERNEL_PROT_GPU_ALL,
			0, //flags
			offsetOut,
			shaderGpuAlignment);
		assert((ret >= 0));


		m_stackBase = (uintptr_t)ptr;
		m_stackMax = m_stackBase + m_stackSize;

		m_stackPointer = m_stackBase;

	}

	void* memAlign(int size, int alignment)
	{
		uint64_t offset = roundup(m_stackPointer, alignment);

		if (offset + size > m_stackMax) return NULL;

		m_stackPointer = offset + size;

		return (void*)(offset);
	}

	void reset(void)
	{
		m_stackPointer = m_stackBase;
	}

	void finish()
	{
		int ret;

		ret = sceKernelReleaseDirectMemory(m_baseOffset, m_stackSize);
		assert((ret >= 0));
	}

private:
	uint64_t m_stackPointer;
	uint32_t m_stackSize;
	uint64_t m_stackMax;
	uint64_t m_stackBase;
	off_t m_baseOffset;
};



/*
 * utility functions for libSceVideoOut
 */
 // setup VideoOut and acquire handle
int32_t initializeVideoOut(SceKernelEqueue* eqFlip)
{
	int videoOutHandle;
	int ret;

	ret = sceVideoOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
	assert((ret >= 0));

	videoOutHandle = ret;

	ret = sceKernelCreateEqueue(eqFlip, "eq to wait flip");
	assert((ret >= 0));

	ret = sceVideoOutAddFlipEvent(*eqFlip, videoOutHandle, NULL);
	assert((ret >= 0));


	return videoOutHandle;
}

// register display buffers
int initializeDisplayBuffer(int32_t videoOutHandle, RenderBuffer* renderBuffers, int num, int startIndex)
{
	assert(SCE_VIDEO_OUT_BUFFER_NUM_MAX >= num);
	void* addresses[SCE_VIDEO_OUT_BUFFER_NUM_MAX];
	for (int i = 0; i < num; i++)
	{
		addresses[i] = renderBuffers[i].getAddress();
	}

	SceVideoOutBufferAttribute attribute;
	renderBuffers[0].getRenderBufferAttributes()->setDisplayBufferAttributes(&attribute);

	// register the buffers to the slot: [startIndex..startIndex+num-1]
	int ret = sceVideoOutRegisterBuffers(videoOutHandle, startIndex, &addresses[0], num, &attribute);
	assert((ret >= 0));

	for (int i = 0; i < num; i++)
	{
		renderBuffers[i].setDisplayBufferIndex(startIndex + i); // set the buffer index to flip
	}

	return ret;
}

// allocate direct memory for display buffers
void allocateDisplayBuffers(VideoMemory* videoMem, RenderBuffer* renderBuffers, int bufferNum, RenderBufferAttributes* attr)
{
	for (int i = 0; i < bufferNum; i++)
	{
		void* addr = videoMem->memAlign(attr->getSize(), attr->getAlign());
		assert((addr != NULL));
		renderBuffers[i].init(attr, addr);
	}
}

// wait until no flip is on pending
void waitFlipAllFinished(int videoOutHandle, SceKernelEqueue* eqFlip)
{
	while (sceVideoOutIsFlipPending(videoOutHandle) != 0) 
	{
		SceKernelEvent ev;
		int out;
		int ret = sceKernelWaitEqueue(*eqFlip, &ev, 1, &out, NULL);
		assert((ret >= 0));
	}
}

// wait until incremental flipArg (specified at SubmitFlip) reaches the value
void waitFlipArg(int videoOutHandle, SceKernelEqueue* eqFlip, int64_t flipArg)
{
	while (true)
	{
		int ret = 0;

		SceVideoOutFlipStatus status;
		ret = sceVideoOutGetFlipStatus(videoOutHandle, &status);
		assert((ret >= 0));

		if (status.flipArg >= flipArg) 
		{
			return;
		}

		SceKernelEvent ev;
		int out;
		ret = sceKernelWaitEqueue(*eqFlip, &ev, 1, &out, 0);
		assert((ret >= 0));
	}
}

static void configureWithResolution(int handle, RenderBufferAttributes* attr)
{
	bool isTile = false;
	int width, height;

	if (sceKernelIsNeoMode())
	{
		SceVideoOutResolutionStatus status;
		if (SCE_OK == sceVideoOutGetResolutionStatus(handle, &status) && status.fullHeight > 1080)
		{
			height = 2160;
		}
		else 
		{
			height = 1080;
		}
	}
	else 
	{
		height = 720;
	}
	width = height * 16 / 9;

	attr->init(width, height, isTile);
}

#pragma endregion

namespace
{
	RenderBuffer renderBuffers[DISPLAY_BUFFER_NUM];
	int32_t videoOutHandle;
	int bufferIndex;
	SceKernelEqueue eqFlip;
	RenderBufferAttributes attr;
	VideoMemory videoMem;
}

static int g_initialUser;
static int g_padHandle; // we're reusing this for the physical keyboard, don't ask.

#ifdef USE_IME_KEYBOARD
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
		const SceImeKeycode& data = e->param.keycode;
		bool isDown = e->id == SCE_IME_KEYBOARD_EVENT_KEYCODE_DOWN;
		AddKeyToQueue(isDown, data.keycode);

		// we have to deal with modifiers separately again... :/
		// we're just using the doom keycodes here, since it's easier.
		//AddKeyToQueue((data.status & SCE_IME_KEYCODE_STATE_MODIFIER_L_SHIFT) != 0, KEY_RSHIFT);
		//AddKeyToQueue((data.status & SCE_IME_KEYCODE_STATE_MODIFIER_R_SHIFT) != 0, KEY_RSHIFT);
		//AddKeyToQueue((data.status & SCE_IME_KEYCODE_STATE_MODIFIER_L_CTRL) != 0, KEY_FIRE);
		//AddKeyToQueue((data.status & SCE_IME_KEYCODE_STATE_MODIFIER_R_CTRL) != 0, KEY_FIRE);
	}
	break;

	default: break;
	}

}
#endif

void DG_Init(void)
{
	int ret = 0;

	ret = sceUserServiceInitialize(NULL);
	assert((ret >= 0));

	ret = sceUserServiceGetInitialUser(&g_initialUser);
	assert((ret >= 0));

#ifdef USE_GAMEPAD_AS_KEYBOARD
	ret = scePadInit();
	assert((ret >= 0));

	ret = scePadOpen(g_initialUser, SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);
	assert((ret >= 0));
	g_padHandle = ret;
#endif

#ifdef USE_PHYSICAL_KEYBOARD
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_RESERVED50);
	assert((ret >= 0));

	ret = sceDbgKeyboardInit();
	assert((ret >= 0));

	ret = sceDbgKeyboardOpen(g_initialUser, SCE_DBG_KEYBOARD_PORT_TYPE_STANDARD, 0, NULL);
	assert((ret >= 0));
	g_padHandle = ret;
#endif

#ifdef USE_IME_KEYBOARD
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_LIBIME);
	assert((ret >= 0));

	SceImeKeyboardParam kparam = {};
	kparam.option = SCE_IME_KEYBOARD_OPTION_DEFAULT;
	kparam.handler = HandleIMEKeyboardEvent;

	ret = sceImeKeyboardOpen(g_initialUser, &kparam);
	assert((ret >= 0));

	g_padHandle = 0; // please the compiler.
#endif

	// setup VideoOut and acquire handle
	videoOutHandle = initializeVideoOut(&eqFlip);

	configureWithResolution(videoOutHandle, &attr);

	// setup direct memory for display buffers
	videoMem.init(VIDEO_MEMORY_STACK_SIZE);

	// allocate display buffers
	allocateDisplayBuffers(&videoMem, renderBuffers, DISPLAY_BUFFER_NUM, &attr);

	// register display buffers
	int startIndex = 0;
	initializeDisplayBuffer(videoOutHandle, renderBuffers, DISPLAY_BUFFER_NUM, startIndex);


	// this should be called regularly
	sce::Gnm::submitDone();

	// set flip conditions
	ret = sceVideoOutSetFlipRate(videoOutHandle, FLIP_RATE);
	assert((ret >= 0));

	// loop setup
	bufferIndex = 0;

	memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(unsigned short));
}

/*
 * simple functions to draw to surface by cpu. EXTREMELY SLOW. Use GPU for real applications.
 */
static void writeColor(RenderBuffer* rb, int x, int y, uint32_t color)
{
	if (!rb) return;

	uintptr_t baseAddress = (uintptr_t)rb->getAddress();

	if (rb->isTile())
	{
		puts("tiled");
		uint64_t offset = rb->getTiledByteOffset(x, y); // this is EXTREMELY SLOW
		*((uint32_t*)(baseAddress + offset)) = color;
	}
	else // linear
	{
		puts("linear");
		int pixel = (y * rb->getPitch()) + x;
		((uint32_t*)baseAddress)[pixel] = color; // this is still very SLOW
	}
}

static void HandleKeyInput(void)
{
	// this is causing random freezes, or at least I think so.

#ifdef USE_GAMEPAD_AS_KEYBOARD
	// this is far from ideal, but we need to deal with inputs somewhere...
	ScePadData data = {};
	scePadReadState(g_padHandle, &data);

#define MAP_KEY(key) AddKeyToQueue((data.buttons & (key)) == (key), (key))

	MAP_KEY(SCE_PAD_BUTTON_CROSS);
	MAP_KEY(SCE_PAD_BUTTON_CIRCLE);
	MAP_KEY(SCE_PAD_BUTTON_SQUARE);
	MAP_KEY(SCE_PAD_BUTTON_TRIANGLE);

	MAP_KEY(SCE_PAD_BUTTON_LEFT);
	MAP_KEY(SCE_PAD_BUTTON_RIGHT);
	MAP_KEY(SCE_PAD_BUTTON_UP);
	MAP_KEY(SCE_PAD_BUTTON_DOWN);

	MAP_KEY(SCE_PAD_BUTTON_R1);
	MAP_KEY(SCE_PAD_BUTTON_L1);

#undef MAP_KEY
#endif

#ifdef USE_PHYSICAL_KEYBOARD
	int ret = 0;

	SceDbgKeyboardData data = { };
	ret = sceDbgKeyboardReadState(g_padHandle, &data);
	if (ret == SCE_OK)
	{
		for (int i = 0; i < data.length; i++)
		{
			if (!data.keyCode[i])
			{
				continue;
			}

			AddKeyToQueue(1, data.keyCode[i]);
		}

		// And now, we need to handle modifier keys, because they're separate. Sigh.
		if ((data.modifierKey & SCE_DBG_KEYBOARD_MKEY_L_CTRL) == SCE_DBG_KEYBOARD_MKEY_L_CTRL)
			AddKeyToQueue(1, SCE_DBG_KEYBOARD_CODE_L_CTRL);

		if ((data.modifierKey & SCE_DBG_KEYBOARD_MKEY_R_CTRL) == SCE_DBG_KEYBOARD_MKEY_R_CTRL)
			AddKeyToQueue(1, SCE_DBG_KEYBOARD_CODE_R_CTRL);

		if ((data.modifierKey & SCE_DBG_KEYBOARD_MKEY_L_SHIFT) == SCE_DBG_KEYBOARD_MKEY_L_SHIFT)
			AddKeyToQueue(1, SCE_DBG_KEYBOARD_CODE_L_SHIFT);

		if ((data.modifierKey & SCE_DBG_KEYBOARD_MKEY_R_SHIFT) == SCE_DBG_KEYBOARD_MKEY_R_SHIFT)
			AddKeyToQueue(1, SCE_DBG_KEYBOARD_CODE_R_SHIFT);
	}
#endif

#ifdef USE_IME_KEYBOARD
	int ret = sceImeUpdate(HandleIMEKeyboardEvent);
	assert((ret >= 0));
#endif
}

void DG_DrawFrame(void)
{
	const int flipArgInitValue = 0; // the system sets flipArg to -1 at sceVideoOutOpen
	static int64_t flipArg = flipArgInitValue;

	int ret = 0;
	RenderBuffer* rb = &renderBuffers[bufferIndex];

	// cpu process which does not override display buffers can be put here before waitFlipArg().

	// wait until the new rendering buffer is free from scan-out.
	// "rb->getLastFlipArg()+1"  means "the next flip (means +1) after the previous flip of this buffer (rb->getLastFlipArg())"
	// so "it has flipped to the next buffer and this buffer is not used anymore"
	// with gnm rendering this can be done by command waitUntilSafeForRendering()
	waitFlipArg(videoOutHandle, &eqFlip, rb->getLastFlipArg() + 1);

	// this is where you'd inject your own code.
	// in our case, we're going to copy doom to the buffer.
	{
		// note: this is slow (compared to the gpu, it's more than good enough for doom though)
		for (int y = 0; y < DOOMGENERIC_RESY; y++)
		{
			for (int x = 0; x < DOOMGENERIC_RESX; x++)
			{
				int pixel = (y * rb->getPitch()) + x;
				((uint32_t*)rb->getAddress())[pixel] = DG_ScreenBuffer[pixel];
			}
		}
	}
	//sceKernelUsleep(10 * 1000);

	// Flush Garlic for memory base synchronization after CPU-write to WC_GARLIC before video-out
	// flushGarlic is NOT necessary if the draw is done by gc or compute shader (but need to flush L2-cache in that case)
	sce::Gnm::flushGarlic();

	// request flip to the buffer
	// with gnm rendering this can be done by submitAndFlipCommandBuffers()
	ret = sceVideoOutSubmitFlip(videoOutHandle, rb->getDisplayBufferIndex(), FLIP_MODE, flipArg);
	assert((ret >= 0));

	rb->setFlipArg(flipArg); // keep the flipArg for this DisplayBuffer

	// increase flipArg for the next loop
	flipArg++;

	// to get the next display buffer
	bufferIndex = (bufferIndex + 1) % DISPLAY_BUFFER_NUM;

	// this should be called regularly
	sce::Gnm::submitDone();

	HandleKeyInput();
}

void DG_SleepMs(uint32_t ms)
{
	static bool no = false;
	if (no)
	{
		writeColor(0, 0, 0, 0);
	}

	sceKernelUsleep(ms * 1000);
}

uint32_t DG_GetTicksMs(void)
{
	/*
	SceRtcTick cur_tick;
	sceRtcGetCurrentTick(&cur_tick);
	return (uint32_t)cur_tick.tick;
	*/

	// inspired by the x11 implementation, doesn't seem to work great on real hw. (too fast)

	SceRtcDateTime cur_date;
	sceRtcGetCurrentClock(&cur_date, 0);
	return cur_date.second * 1000 + cur_date.microsecond / 1000; 
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
	if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
	{
		//key queue is empty
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

	static bool running = true;
	while (running)
	{
		doomgeneric_Tick();
	}

#ifdef USE_GAMEPAD_AS_KEYBOARD
	scePadClose(g_padHandle);
#endif

	// wait until all flips done.
	waitFlipAllFinished(videoOutHandle, &eqFlip);

	sce::Gnm::submitDone();

	sleep(1); // in seconds

	sce::Gnm::submitDone();

	// blank screen and unregister buffers
	// with gnm rendering, this should be done AFTER all gc commands are finished.
	sceVideoOutClose(videoOutHandle);

	// free all buffers allcoated by videoMalloc()/videoMemAlign().
	videoMem.reset();

	// release the direct memory for display buffers
	videoMem.finish();

	printf("Finished\n");
}
#endif // __ORBIS__