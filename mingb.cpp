#include <windows.h>
#include <cinttypes>
#include <stdio.h>
#include <malloc.h>

static int SCREEN_WIDTH = 160;
static int SCREEN_HEIGHT = 144;
static int SCREEN_SCALE = 4;
static float MS_PER_FRAME = 1000.0f / 60.0f;

#define SET(x,b)    x|=b
#define RESET(x,b)  x&=~(b)
#define TOGGLE(x,b) x^=b

#define FLAG_JOY_RIGHT   1 << 0 // 0000 0001 
#define FLAG_JOY_LEFT    1 << 1 // 0000 0010
#define FLAG_JOY_UP      1 << 2 // 0000 0100
#define FLAG_JOY_DOWN    1 << 3 // 0000 1000
#define FLAG_JOY_A       1 << 4 // 0001 0000
#define FLAG_JOY_B       1 << 5 // 0010 0000
#define FLAG_JOY_SELECT  1 << 6 // 0100 0000
#define FLAG_JOY_START   1 << 7 // 1000 0000

#define TILE_WIDTH       8
#define TILE_HEIGHT	     8
#define TILE_MAP_WIDTH   5
#define TILE_MAP_HEIGHT  4

uint8_t inputflags = 0;

HWND windowHandle;
BITMAPINFO BitmapInfo;
void* BitmapMemory;
int BitmapWidth;
int BitmapHeight;

typedef uint8_t Tile[8][8];

Tile Tiles[3] =  {  2,0,3,2,2,0,3,2,
					2,0,2,2,2,0,2,2,
					0,0,0,0,0,0,0,0,
					3,2,2,0,3,2,2,0,
					2,2,2,0,2,2,2,0,
					0,0,0,0,0,0,0,0,
					2,0,3,2,2,0,3,2,
					2,0,2,2,2,0,2,2,

				    0,0,0,0,0,0,0,0,
					0,2,2,2,2,2,2,0,
					0,2,3,3,3,3,2,0,
					0,2,3,2,2,0,2,0,
					0,2,3,2,2,0,2,0,
					0,2,0,0,0,0,2,0,
					0,2,2,2,2,2,2,0,
					0,0,0,0,0,0,0,0,

				    0,0,0,2,2,2,3,1,
					0,0,0,2,2,2,3,1,
					0,0,0,2,2,2,3,1,
					0,0,0,2,2,2,3,1,
					0,0,0,2,2,2,3,1,
					0,0,0,2,2,2,3,1,
					0,0,0,2,2,2,3,1,
					0,0,0,2,2,2,3,1  };

uint8_t TileMap[TILE_MAP_HEIGHT][TILE_MAP_WIDTH] =   {  0,1,1,1,0,
														0,2,2,2,0,
														0,2,2,2,0,
														0,1,1,1,0  };

#define numColours 4

RGBQUAD Palette[4];

byte* ROM;

void GBResizeDIB(int Width, int Height)
{
	if (BitmapMemory)
	{
		VirtualFree(BitmapMemory, 0, MEM_RELEASE);
	}

	BitmapWidth = TILE_WIDTH;
	BitmapHeight = TILE_HEIGHT;

	BitmapInfo.bmiHeader =
	{
		sizeof(BitmapInfo.bmiHeader),
		BitmapWidth,
		-BitmapHeight,
		1,
		32,
		BI_RGB,
		0,
		0,
		0,
		0,
		0
	};

	for (int i = 0; i < numColours; i++)
	{
		uint8_t col = (UINT8_MAX / (numColours - 1) * i);
		RGBQUAD rgb = { col,col,col,0 };
		Palette[i] = rgb;
	}

	int BytesPerPixel = 4;
	int BitmapMemorySize = (BitmapWidth * BitmapHeight) * BytesPerPixel;
	BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	if (BitmapMemory)
	{
		int Pitch = BitmapWidth * BytesPerPixel;
		uint8_t* Row = (uint8_t*)BitmapMemory;
		for (int Y = 0; Y < BitmapHeight; Y++)
		{
			RGBQUAD* Pixel = (RGBQUAD*)Row;
			for (int X = 0; X < BitmapWidth; X++)
			{
				// BGRx
				auto tile = Tiles[0];
				uint8_t i = tile[Y][X];
				RGBQUAD col = Palette[i];

				memcpy(Pixel, &col, sizeof RGBQUAD);

				Pixel++;
			}

			Row += Pitch;
		}
	}
	// log errors and stuff
}

void RenderFrame(HDC DeviceContext, RECT* WindowRect, int x, int y, int width, int height)
{
	int WindowWidth = WindowRect->right - WindowRect->left;
	int WindowHeight = WindowRect->bottom - WindowRect->top;
	StretchDIBits
	(
		DeviceContext,
		x, y, WindowWidth, WindowHeight,
		x, y, BitmapWidth, BitmapHeight,
		BitmapMemory,
		&BitmapInfo,
		DIB_RGB_COLORS,
		SRCCOPY
	);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SIZE:
	{
		RECT windowRect;
		GetClientRect(windowHandle, &windowRect);
		int Width = windowRect.right - windowRect.left;
		int Height = windowRect.bottom - windowRect.top;
		GBResizeDIB(Width, Height);
		InvalidateRect(hwnd, &windowRect, false);
		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC WinDC;

		WinDC = BeginPaint(hwnd, &ps);
		int X = ps.rcPaint.left;
		int Y = ps.rcPaint.top;
		int Width = ps.rcPaint.right - ps.rcPaint.left;
		int Height = ps.rcPaint.bottom - ps.rcPaint.top;

		//tidy 
		RECT windowRect;
		GetClientRect(windowHandle, &windowRect);

		RenderFrame(WinDC, &windowRect, X, Y, Width, Height);

		EndPaint(hwnd, &ps);
		return 0;
	}
	// TODO:: Associative array for remappable keys
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
			case (unsigned int)('W') :
			{
				// Process the W key.
				SET(inputflags, FLAG_JOY_UP);
				break;
			}
			case (unsigned int)('A') :
			{
				// Process the A key.
				SET(inputflags, FLAG_JOY_LEFT);
				break;
			}
			case (unsigned int)('S') :
			{
				// Process the S key.
				SET(inputflags, FLAG_JOY_DOWN);
				break;
			}
			case (unsigned int)('D') :
			{
				// Process the D key.
				SET(inputflags, FLAG_JOY_RIGHT);
				break;
			}
			case VK_SPACE:
			{
				// Process the Spacebar.
				SET(inputflags, FLAG_JOY_START);
				break;
			}
			case VK_SHIFT:
			{
				// Process the Shift key.
				SET(inputflags, FLAG_JOY_SELECT);;
				break;
			}
			default:
				break;
		}
	}
	case WM_KEYUP:
		switch (wParam)
		{
			case (unsigned int)('W') :
			{
				// Process the W key.
				RESET(inputflags, FLAG_JOY_UP);
				break;
			}
			case (unsigned int)('A') :
			{
				// Process the A key.
				RESET(inputflags, FLAG_JOY_LEFT);
				break;
			}
			case (unsigned int)('S') :
			{
				// Process the S key.
				RESET(inputflags, FLAG_JOY_DOWN);
				break;
			}
			case (unsigned int)('D') :
			{
				// Process the D key.
				RESET(inputflags, FLAG_JOY_RIGHT);
				break;
			}
			case VK_SPACE:
			{
				// Process the Spacebar.
				RESET(inputflags, FLAG_JOY_START);
				break;
			}
			case VK_SHIFT:
			{
				// Process the Shift key.
				RESET(inputflags, FLAG_JOY_SELECT);
				break;
			}
			default:
				break;
		}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void update()
{

}

bool InitWindow(HINSTANCE _hInstance)
{
	WNDCLASS wc = { };

	wc.lpfnWndProc = MainWndProc;
	wc.hInstance = _hInstance;
	wc.lpszClassName = "gbMainWindow";

	RegisterClassA(&wc);

	RECT wr = { 0, 0, SCREEN_WIDTH * SCREEN_SCALE, SCREEN_HEIGHT * SCREEN_SCALE };    // set the size, but not the position
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);    // adjust the size

	windowHandle = CreateWindowExA(
		0,
		"gbMainWindow",
		"MinGB",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wr.right - wr.left,    // width of the window
		wr.bottom - wr.top,    // height of the window
		NULL,
		NULL,
		_hInstance,
		NULL
	);

	if (windowHandle == NULL)
	{
		return false;
	}

	return true;
}

bool LoadROM()
{
	if (ROM)
	{
		VirtualFree(ROM, 0, MEM_RELEASE);
	}

	FILE* RomFile;
	int Filesize;
	fopen_s(&RomFile, "tetris.gb", "r");

	if (RomFile == NULL) {
		fprintf(stderr, "Can't open input file in.list!\n");
		return false;
	}
	fseek(RomFile, 0, SEEK_END);
	Filesize = ftell(RomFile);
	fseek(RomFile, 0, SEEK_SET);

	ROM = (byte*)VirtualAlloc(0, Filesize, MEM_COMMIT, PAGE_READWRITE);
	if (ROM)
	{
		fread((byte*)ROM, sizeof(byte), Filesize, RomFile);
		fclose(RomFile);
	}
	return true;
}

void CleanUp()
{
	if (ROM)
	{
		VirtualFree(ROM, 0, MEM_RELEASE);
	}
	// probably move other cleanup to here as well
}


INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR lpCmdLine, INT nCmdShow)
{
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	uint32_t PerfCountFrequency = PerfCountFrequencyResult.QuadPart;


	if (InitWindow(hInstance))
	{
		ShowWindow(windowHandle, nCmdShow);
	}
	else
	{
		return 0;
	}


	if (!LoadROM())
	{
		return 0;
	}

	MSG msg = { };
	// Enter the infinite message loop
	while (TRUE)
	{
		// Check to see if any messages are waiting in the queue
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// Translate the message and dispatch it to WindowProc()
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			// If the message is WM_QUIT, exit the while loop
			if (msg.message == WM_QUIT)
				break;

			LARGE_INTEGER startCounter;
			QueryPerformanceCounter(&startCounter);

			update();
			// tidy
			RECT windowRect;
			GetClientRect(windowHandle, &windowRect);
			InvalidateRect(windowHandle, &windowRect, true);

			LARGE_INTEGER endCounter;
			QueryPerformanceCounter(&endCounter);
			uint64_t counterElapsed = endCounter.QuadPart - startCounter.QuadPart;
			float deltaTime = ((1000.0f * (float)counterElapsed) / (float)PerfCountFrequency);

			Sleep(MS_PER_FRAME - deltaTime);

		}


		CleanUp();
	}
	return msg.wParam;
}

