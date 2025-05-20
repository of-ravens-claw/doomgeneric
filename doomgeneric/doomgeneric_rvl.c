// DoomGeneric for Revolution
// Please note that the current implementation works, insofar as the game boots and runs*
// However, it seems to freeze shortly after running, and, for some reason, the viewport is sideways.
// It is also very laggy, however I believe that to be due to using the GX api to render.
//
// I believe it would be faster if we modified this to directly write to the XFB instead.
// However, for now, this is good enough*.

#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// revolution includes
#include <revolution/dvd.h>
#include <revolution/os.h>
#include <revolution/gx.h>
#include <revolution/vi.h>
#include <revolution/pad.h> // GameCube controllers

#define DEMO_USE_MEMLIB 1
#include <demo.h> // TEMPORARY (maybe)

#define KEYQUEUE_SIZE 32

static uint16_t s_KeyQueue[KEYQUEUE_SIZE];
static uint32_t s_KeyQueueWriteIndex = 0;
static uint32_t s_KeyQueueReadIndex = 0;

static void AddKeyToQueue(bool pressed, uint8_t keyCode)
{
	s_KeyQueue[s_KeyQueueWriteIndex] = (uint16_t)(pressed << 8 | keyCode);
	s_KeyQueueWriteIndex++;
	if (s_KeyQueueWriteIndex == KEYQUEUE_SIZE)
		s_KeyQueueWriteIndex = 0;
}

static void HandleKeyInput(void)
{
	PADStatus data[PAD_MAX_CONTROLLERS];
	memset(&data, 0, sizeof(data));
	
	PADRead(data);
	
	// Only read the first controller
	if (data[0].err != PAD_ERR_NONE)
		return;
	
	AddKeyToQueue((data[0].button & PAD_BUTTON_B) == PAD_BUTTON_B, KEY_ESCAPE);
	AddKeyToQueue((data[0].button & PAD_BUTTON_A) == PAD_BUTTON_A, KEY_ENTER);

	AddKeyToQueue((data[0].button & PAD_BUTTON_UP) == PAD_BUTTON_UP, KEY_UPARROW);
	AddKeyToQueue((data[0].button & PAD_BUTTON_LEFT) == PAD_BUTTON_LEFT, KEY_LEFTARROW);
	AddKeyToQueue((data[0].button & PAD_BUTTON_RIGHT) == PAD_BUTTON_RIGHT, KEY_RIGHTARROW);
	AddKeyToQueue((data[0].button & PAD_BUTTON_DOWN) == PAD_BUTTON_DOWN, KEY_DOWNARROW);

	AddKeyToQueue((data[0].button & PAD_TRIGGER_R) == PAD_TRIGGER_R, KEY_FIRE);
	AddKeyToQueue((data[0].button & PAD_TRIGGER_L) == PAD_TRIGGER_L, KEY_USE);
}

static BOOL allocators_valid = FALSE;

#if 0
// easily copied, so you can use it on other files.
#include <revolution/types.h> // for BOOL
void* RVLMalloc(size_t size, BOOL mem1);
void* RVLCalloc(size_t num, size_t size, BOOL mem1);
void RVLFree(void* ptr, BOOL mem1);
#endif

void* RVLMalloc(size_t size, BOOL mem1)
{
	if (!allocators_valid)
		return NULL;

	return MEMAllocFromAllocator(mem1 ? &DemoAllocator1 : &DemoAllocator2, size);
}

void* RVLCalloc(size_t num, size_t size, BOOL mem1)
{
	void* ptr = RVLMalloc(num * size, mem1);
	if (ptr)
		memset(ptr, 0, num * size);
	return ptr;
}

void RVLFree(void* ptr, BOOL mem1)
{
	if (!allocators_valid || !ptr)
		return;

	MEMFreeToAllocator(mem1 ? &DemoAllocator1 : &DemoAllocator2, ptr);
}

// Taken from https://github.com/xtreme8000/CavEX/blob/master/source/platform/wii/texture.c#L70
// Modified slightly
#define xy16(x, y) (((int)(x) << 8) | (int)(y))
static void tex_conv_rgba32(u8* dst, const u32* src, u16 width, u16 height)
{
    // Adapt code to use different inputs
    uint8_t*  image  = (uint8_t*)src;
    uint16_t* output = (uint16_t*)dst;
    
	size_t output_idx = 0;
	for (size_t y = 0; y < height; y += 4)
	{
		for (size_t x = 0; x < width; x += 4)
		{
		    // First plane
			for (size_t by = 0; by < 4; by++)
			{
				for (size_t bx = 0; bx < 4; bx++)
				{
					uint8_t* col = image + (x + bx + (y + by) * width) * 4;
					output[output_idx++] = xy16(col[3], col[0]);
				}
			}

			// Second plane
			for (size_t by = 0; by < 4; by++)
			{
				for (size_t bx = 0; bx < 4; bx++)
				{
					uint8_t* col = image + (x + bx + (y + by) * width) * 4;
					output[output_idx++] = xy16(col[1], col[2]);
				}
			}
		}
	}

	DCFlushRange(output, width * height * 4);
}
#undef xy16

// Copy of DG_ScreenBuffer, but in 4x4 tiles, that is, the GPU's native texture format.
uint8_t* DG_RVLScreenBuffer = NULL;
GXTexObj DG_ScreenTexture;

void DG_Init(void)
{
	DEMOInit(NULL); // Will initialize GX for us.
	PADInit();
	allocators_valid = TRUE;

	memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(uint16_t));

	// This one is used by the game directly
	DG_ScreenBuffer = RVLMalloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4, TRUE);
	
	// This one is only used by us
	const size_t rvlBufSize = GXGetTexBufferSize(DOOMGENERIC_RESX, DOOMGENERIC_RESY, GX_TF_RGBA8, GX_FALSE, 1);
	DG_RVLScreenBuffer = RVLMalloc(rvlBufSize, TRUE);
	
	// Create our texture
	GXInitTexObj(&DG_ScreenTexture, DG_RVLScreenBuffer, DOOMGENERIC_RESX, DOOMGENERIC_RESY, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE); // Last 3 don't matter.
    GXInitTexObjLOD(&DG_ScreenTexture, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1); // Is this needed?
	
	// Setup vertex configuration
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U16, 0);

    // ?
    GXSetNumChans(1);
    GXSetChanCtrl(GX_ALPHA0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    
    // Set texture coordinate generation
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	
	// Setup texture environment
	GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    
    // Setup viewport, orthographic projection matrix
    GXSetViewport(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f); // <-- done by DEMOBeforeRender
    
    Mtx44 pMtx;
    MTXOrtho(pMtx, 0.0f, 640.0f, 0.0f, 480.0f, 0.0f, -100.0f);
    GXSetProjection(pMtx, GX_ORTHOGRAPHIC);

    Mtx mMtx;
    MTXIdentity(mMtx);
    GXLoadPosMtxImm(mMtx, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    
    // And finally, set a clear color
    // We use black for this.
    GXColor clear_color_test = { 0, 0, 0, 0 };
    GXSetCopyClear(clear_color_test, GX_MAX_Z24);
    
    // This must be done once at init!
    // (otherwise we will never clear VIBlack)
    for (int i = 0; i < 4; i++)
    {
    	DEMOBeforeRender();
    	DEMODoneRender();
    }
}

void DG_DrawFrame(void)
{
	DEMOBeforeRender();

	HandleKeyInput();

	// Update our texture
	// NOTE: Wasteful. It would be best to make Doom write the pixel data the way we want directly.
	tex_conv_rgba32(DG_RVLScreenBuffer, DG_ScreenBuffer, DOOMGENERIC_RESX, DOOMGENERIC_RESY);
	
	// Render
	GXInvalidateTexAll();
	GXLoadTexObj(&DG_ScreenTexture, GX_TEXMAP0);
	
	// TODO: Renders fucking sideways...
	// The simplest way to fix it would probably be to rotate the projection matrix?
	// couldn't get that working though, it just displayed black...
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	{
		GXPosition2s16(0, 0);
		GXTexCoord2u16(0, 1);
		
		GXPosition2s16(DOOMGENERIC_RESY, 0);
		GXTexCoord2u16(0, 0);
		
		GXPosition2s16(DOOMGENERIC_RESY, DOOMGENERIC_RESX);
		GXTexCoord2u16(1, 0);
		
		GXPosition2s16(0, DOOMGENERIC_RESX);
		GXTexCoord2u16(1, 1);
	}
	GXEnd();

	// TODO: Unfortunately this causes terrible lag because of waiting for the VI interrupt.
	DEMODoneRender(); // Copies the EFB into the XFB
}

void DG_SleepMs(uint32_t ms)
{
	OSSleepMilliseconds(ms);
}

uint32_t DG_GetTicksMs(void)
{
	return OSTicksToMilliseconds(OSGetTick());
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
	if (s_KeyQueueReadIndex == KEYQUEUE_SIZE)
		s_KeyQueueReadIndex = 0;

	*pressed = keyData >> 8;
	*doomKey = (uint8_t)(keyData & 0xFF);

	return 1;
}

void DG_SetWindowTitle(const char* title)
{
	OSReport("DG_SetWindowTitle: '%s'\n", title);
}

int main(void)
{
	static const int argc = 3;
	static const char* argv[] = { "doomgeneric", "-iwad", "DOOM.WAD" };
	doomgeneric_Create(argc, argv);

	while (1)
	{
		doomgeneric_Tick();
	}
	
	OSHalt("end of demo program");
}