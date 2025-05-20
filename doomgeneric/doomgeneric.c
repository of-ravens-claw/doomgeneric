#include <stdio.h>
#include <stdlib.h>

#include "m_argv.h"
#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;
void D_DoomMain(void);

void doomgeneric_Create(int argc, char **argv)
{
	printf("%d command-line args:\n", argc);
	for (int k = 0; k < argc; k++)
	{
		printf("'%s'\n", argv[k]);
	}

	// save arguments
    myargc = argc;
    myargv = argv;
	M_FindResponseFile();

// TODO: Remove this from ALL platforms!
#ifndef RVL
	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
#endif

	DG_Init();
	D_DoomMain();
}
