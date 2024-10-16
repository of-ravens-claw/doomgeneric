#ifndef DOOM_GENERIC
#define DOOM_GENERIC

#include <stdint.h>

#if defined(__ORBIS__)
#define DOOMGENERIC_RESX   (1920)
#define DOOMGENERIC_RESY   (1080)
#elif defined(__psp2__)
#define DOOMGENERIC_RESX   (960)
#define DOOMGENERIC_RESY   (544)
#else
#define DOOMGENERIC_RESX   (640)
#define DOOMGENERIC_RESY   (400)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CMAP256
typedef uint8_t pixel_t;
#else
typedef uint32_t pixel_t;
#endif

extern pixel_t* DG_ScreenBuffer;

void doomgeneric_Create(int argc, char **argv);
void doomgeneric_Tick(void);

// Implement the following functions for your platform:
void     DG_Init(void);
void     DG_DrawFrame(void);
void     DG_SleepMs(uint32_t ms);
uint32_t DG_GetTicksMs(void);
int      DG_GetKey(int* pressed, unsigned char* key);
void     DG_SetWindowTitle(const char* title);

#ifdef __cplusplus
}
#endif

#endif //DOOM_GENERIC
