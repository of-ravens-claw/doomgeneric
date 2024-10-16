#ifdef _WIN32

#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <Windows.h>

static BITMAPINFO s_Bmi = 
{
	sizeof(BITMAPINFOHEADER),
	DOOMGENERIC_RESX,
	-DOOMGENERIC_RESY,
	1,
	32
};
static HWND s_Hwnd = NULL;
static HDC s_Hdc = NULL;

#define KEYQUEUE_SIZE 16

static uint16_t s_KeyQueue[KEYQUEUE_SIZE];
static uint32_t s_KeyQueueWriteIndex = 0;
static uint32_t s_KeyQueueReadIndex = 0;

static uint8_t ConvertToDoomKey(uint8_t key)
{
	switch (key)
	{
	case VK_RETURN:
		key = KEY_ENTER;
		break;
	case VK_ESCAPE:
		key = KEY_ESCAPE;
		break;
	case VK_LEFT:
		key = KEY_LEFTARROW;
		break;
	case VK_RIGHT:
		key = KEY_RIGHTARROW;
		break;
	case VK_UP:
		key = KEY_UPARROW;
		break;
	case VK_DOWN:
		key = KEY_DOWNARROW;
		break;
	case VK_CONTROL:
		key = KEY_FIRE;
		break;
	case VK_SPACE:
		key = KEY_USE;
		break;
	case VK_SHIFT:
		key = KEY_RSHIFT;
		break;
	case VK_OEM_COMMA:
		key = KEY_STRAFE_L;
		break;
	case VK_OEM_PERIOD:
		key = KEY_STRAFE_R;
		break;
	case VK_TAB:
		key = KEY_TAB;
		break;
	case VK_BACK:
		key = KEY_BACKSPACE;
		break;
	default:
		key = tolower(key);
		break;
	}

	return key;
}

static void AddKeyToQueue(int pressed, unsigned char keyCode)
{
	//printf("[AddKeyToQueue]: Key 0x%02X, press: %d\n", keyCode, pressed);

	uint8_t key = ConvertToDoomKey(keyCode);
	uint16_t keyData = (pressed << 8) | key;

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	default: break;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		ExitProcess(0);

	case WM_KEYDOWN:
		AddKeyToQueue(TRUE, wParam & 0xFF);
		break;

	case WM_KEYUP:
		AddKeyToQueue(FALSE, wParam & 0xFF);
		break;
	}

	return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void DG_Init(void)
{
	WNDCLASSEXA wc = 
	{
		.cbSize = sizeof(WNDCLASSEXA),
		.lpfnWndProc = WndProc,
		.lpszClassName = "DoomClass"
	};
	if (!RegisterClassExA(&wc))
	{
		printf("Window Registration Failed!");
		exit(-1);
	}

	RECT rect = 
	{
		0,
		0,
		DOOMGENERIC_RESX,
		DOOMGENERIC_RESY
	};
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

	HWND hwnd = CreateWindowExA(0, "DoomClass", "Doom", 
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top, 
		NULL, NULL, NULL, NULL);
	if (hwnd)
	{
		s_Hwnd = hwnd;
		s_Hdc = GetDC(hwnd);
		ShowWindow(hwnd, SW_SHOW);
	}
	else
	{
		printf("Window Creation Failed!");
		exit(-1);
	}

	memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(uint16_t));
}

void DG_DrawFrame(void)
{
	MSG msg;
	memset(&msg, 0, sizeof(msg));

	while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
	
	StretchDIBits(s_Hdc, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, DG_ScreenBuffer, &s_Bmi, 0, SRCCOPY);
	SwapBuffers(s_Hdc);
}

void DG_SleepMs(uint32_t ms)
{
	Sleep(ms);
}

uint32_t DG_GetTicksMs(void)
{
	LARGE_INTEGER counter;
	LARGE_INTEGER freq;
	QueryPerformanceCounter(&counter);
	QueryPerformanceFrequency(&freq);

	return (counter.QuadPart * 1000) / freq.QuadPart; 
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
	if (s_Hwnd) SetWindowTextA(s_Hwnd, title);
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