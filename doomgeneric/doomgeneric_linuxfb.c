#ifdef __linux__

#include "doomkeys.h"
#include "doomgeneric.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define KEYQUEUE_SIZE 16

static uint16_t s_KeyQueue[KEYQUEUE_SIZE];
static uint32_t s_KeyQueueWriteIndex = 0;
static uint32_t s_KeyQueueReadIndex = 0;

static uint8_t ConvertToDoomKey(uint32_t key)
{
	return key & 0xFF;
}

static void AddKeyToQueue(int pressed, uint32_t keyCode)
{
	uint8_t key = ConvertToDoomKey(keyCode);
	uint16_t keyData = (pressed << 8) | key;

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static void* s_FrameBufferData = NULL;
static int s_FrameBufferDataSize = 0;

#define expect(val, expected) if ((val) == (expected)) { printf("Value %d (%s) matches expected value %d!\n", val, #val, expected); } else { printf("Value %d (%s) DOES NOT MATCH expected value %d!\n", val, #val, expected);}

void DG_Init(void)
{
	memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(uint16_t));

	int fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd < 0)
	{
		printf("failed to open /dev/fb0, maybe try running as root?\n");
		abort();
	}

	struct fb_var_screeninfo vinfo;
	ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);

	int fb_width = vinfo.xres;
	int fb_height = vinfo.yres;
	int fb_bpp = vinfo.bits_per_pixel;
	int fb_bytes = fb_bpp / 8;

	printf("fb_width:  %d\n", fb_width);
	printf("fb_height: %d\n", fb_height);
	printf("fb_bpp:    %d\n", fb_bpp);
	printf("fb_bytes:  %d\n", fb_bytes);

	expect(fp_bpp, 32);

	printf("Please modify doomgeneric.h with the correct values.\n");
	printf("To be specific, change DOOMGENERIC_RESX to the value of fb_width, and DOOMGENERIC_RESY to the value of fb_height.\n");
	printf("Doom will not work otherwise. I would like to make this dynamic, but I fear it would take too many changes to the code.\n");
	printf("Perhaps I should just ditch doomgeneric and make my own thing that supports all the platforms I want...\n");
	printf("Probably based out of a newer version of chocolate-doom, or maybe something else.\n");
	printf("\n");
	printf("Most would probably call be insane for considering the original source code release, but :)\n");
	printf("\n");
	printf("Anyway, once you've modified it, comment the next line out.\n");
	exit(0);

	s_FrameBufferDataSize = fb_width * fb_height * fb_bytes;
	s_FrameBufferData = mmap(0, s_FrameBufferDataSize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, (off_t)0);
	memset(s_FrameBufferData, 0, s_FrameBufferDataSize);
}


void DG_DrawFrame(void)
{
	// Maybe that's all we need?
	memcpy(s_FrameBufferData, DG_ScreenBuffer, s_FrameBufferDataSize);
}

void DG_SleepMs(uint32_t ms)
{
	 usleep(ms * 1000);
}

uint32_t DG_GetTicksMs(void)
{
	struct timeval  tp;
	struct timezone tzp;

	gettimeofday(&tp, &tzp);
	return (tp.tv_sec * 1000) + (tp.tv_usec / 1000); /* return milliseconds */
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