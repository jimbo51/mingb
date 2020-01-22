#include <windows.h>
#include <cinttypes>
#include <stdio.h>
#include <malloc.h>
#include <assert.h>

static int SCREEN_WIDTH = 160;
static int SCREEN_HEIGHT = 144;
static int SCREEN_SCALE = 4;
static float MS_PER_FRAME = 1000.0f / 60.0f;

#define SET(x,b)    x|=b
#define RESET(x,b)  x&=~(b)
#define TOGGLE(x,b) x^=b
#define TEST(x,b)	x&b

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
#define TILE_MAP_WIDTH   10
#define TILE_MAP_HEIGHT  8

uint8_t InputFlags = 0;

HWND windowHandle;
BITMAPINFO BitmapInfo;
void* BitmapMemory;
int BitmapWidth;
int BitmapHeight;

typedef uint8_t Tile[8][8];

Tile Tiles[3] =  {	2,0,3,2,2,0,3,2,
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

uint8_t TileMap[TILE_MAP_HEIGHT][TILE_MAP_WIDTH] =   {  1, 2, 1, 1, 0, 0, 1, 1, 2, 1,
														0, 2, 2, 2, 0, 0, 2, 2, 2, 0,
														0, 2, 0, 2, 0, 0, 2, 0, 2, 0,
														0, 1, 1, 1, 0, 0, 1, 1, 1, 0,
														1, 2, 1, 1, 0, 0, 1, 1, 2, 1,
														0, 2, 2, 2, 0, 0, 2, 2, 2, 0,
														0, 2, 0, 2, 0, 0, 2, 0, 2, 0,
														0, 1, 1, 1, 0, 0, 1, 1, 1, 0  };

#define NUM_COLOURS 4

#define CYCLES_PER_FRAME 69905

RGBQUAD Palette[4];

byte* RAM;

#define FLAG_Z		1 << 7
#define FLAG_N		1 << 6
#define FLAG_H		1 << 5
#define FLAG_C		1 << 4

static union {
	struct {
		uint8_t StatusFlags;
		uint8_t A;
	};
	uint16_t AF;
};

static union {
	struct {
		uint8_t C;
		uint8_t B;
	};
	uint16_t BC;
};

static union {
	struct {
		uint8_t E;
		uint8_t D;
	};
	uint16_t DE;
};

static union {
	struct {
		uint8_t L;
		uint8_t H;
	};
	uint16_t HL;
};

uint16_t* StackPointer;
uint16_t ProgramCounter;

void GBResizeDIB(int Width, int Height)
{
	if (BitmapMemory)
	{
		VirtualFree(BitmapMemory, 0, MEM_RELEASE);
	}

	BitmapWidth = TILE_WIDTH * TILE_MAP_WIDTH;
	BitmapHeight = TILE_HEIGHT * TILE_MAP_HEIGHT;

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

	for (int i = 0; i < NUM_COLOURS; i++)
	{
		uint8_t col = (UINT8_MAX / (NUM_COLOURS - 1) * i);
		RGBQUAD rgb = { col,col,col,0 };
		Palette[i] = rgb;
	}

	int BitmapMemorySize = (BitmapWidth * BitmapHeight) * sizeof(uint32_t);
	BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	if (BitmapMemory)
	{
		RGBQUAD* Pixel = (RGBQUAD*)BitmapMemory;
		for (int TileY = 0; TileY < TILE_MAP_HEIGHT; TileY++)
		{
			for (int Y = 0; Y < TILE_HEIGHT; Y++)
				// NOTE loop incrementing Y comes outside loop inc'ing TileX
				// for contiguous pixels
			{
				for (int TileX = 0; TileX < TILE_MAP_WIDTH; TileX++)
				{
					int TileI = TileMap[TileY][TileX];
					//loop to draw bitmap as before
					auto tile = Tiles[TileI];
					for (int X = 0; X < TILE_WIDTH; X++)
					{
						uint8_t ColI = tile[Y][X];
						//printf("%d", ColI);
						RGBQUAD ColBGR = Palette[ColI];
						memcpy(Pixel, &ColBGR, sizeof(RGBQUAD));
						Pixel++;
					}
				}
				//printf("\n");
			}
		}	
	}
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
				SET(InputFlags, FLAG_JOY_UP);
				break;
			}
			case (unsigned int)('A') :
			{
				// Process the A key.
				SET(InputFlags, FLAG_JOY_LEFT);
				break;
			}
			case (unsigned int)('S') :
			{
				// Process the S key.
				SET(InputFlags, FLAG_JOY_DOWN);
				break;
			}
			case (unsigned int)('D') :
			{
				// Process the D key.
				SET(InputFlags, FLAG_JOY_RIGHT);
				break;
			}
			case VK_SPACE:
			{
				// Process the Spacebar.
				SET(InputFlags, FLAG_JOY_START);
				break;
			}
			case VK_SHIFT:
			{
				// Process the Shift key.
				SET(InputFlags, FLAG_JOY_SELECT);;
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
				RESET(InputFlags, FLAG_JOY_UP);
				break;
			}
			case (unsigned int)('A') :
			{
				// Process the A key.
				RESET(InputFlags, FLAG_JOY_LEFT);
				break;
			}
			case (unsigned int)('S') :
			{
				// Process the S key.
				RESET(InputFlags, FLAG_JOY_DOWN);
				break;
			}
			case (unsigned int)('D') :
			{
				// Process the D key.
				RESET(InputFlags, FLAG_JOY_RIGHT);
				break;
			}
			case VK_SPACE:
			{
				// Process the Spacebar.
				RESET(InputFlags, FLAG_JOY_START);
				break;
			}
			case VK_SHIFT:
			{
				// Process the Shift key.
				RESET(InputFlags, FLAG_JOY_SELECT);
				break;
			}
			default:
				break;
		}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

uint16_t ReadU16()
{
	ProgramCounter++;
	uint8_t lo = RAM[ProgramCounter];
	ProgramCounter++;
	uint8_t hi = RAM[ProgramCounter];
	ProgramCounter++;

	return hi << 8 | lo;
}

uint8_t ReadU8()
{
	ProgramCounter++;
	uint8_t n = RAM[ProgramCounter];
	ProgramCounter++;
	return n;
}

void WriteU8(uint16_t addr, uint8_t n)
{
	RAM[addr] = n;
	ProgramCounter++;
}

int8_t ReadI8()
{
	ProgramCounter++;
	int8_t n = RAM[ProgramCounter];
	ProgramCounter++;
	return n;
}

void UpdateZeroFlag(uint8_t a)
{

	if (a == 0)
	{
		SET(StatusFlags, FLAG_Z);
	}
	else
	{
		RESET(StatusFlags, FLAG_Z);
	}
}

void UpdateHalfCarryFlag(int8_t a, int8_t b)
{
	if ((((a & 0xf) + (b & 0xf)) & 0x10) == 0x10)
	{
		SET(StatusFlags, FLAG_H);
	}
	else
	{
		RESET(StatusFlags, FLAG_H);
	}
}

void update()
{
	uint32_t CycleCount = 0; 
	uint8_t opcode;

	while (CycleCount <= CYCLES_PER_FRAME)
	{
		// Fetch
		opcode = RAM[ProgramCounter];

		// Execute
		switch (opcode)
		{
		case 0x00: // NOP
		{
			ProgramCounter++;
			CycleCount += 4;
			break;
		}
		case 0xC3: // JP a16
		{
			uint16_t nn = ReadU16();
			ProgramCounter = nn;
			CycleCount += 16;
			break;
		}
		case 0xAF: // XOR A
		{
			A = A ^ A;
			ProgramCounter++;
			RESET(StatusFlags, FLAG_N|FLAG_H|FLAG_C);
			UpdateZeroFlag(A);
			CycleCount += 4;
			break;
		}
		case 0x21: // LD HL,d16
		{
			uint16_t nn = ReadU16();
			HL = nn;
			CycleCount += 12;
			break;
		}
		case 0x0E: // LD C,d8
		{
			uint8_t n = ReadU8();
			C = n;
			CycleCount += 8;
			break;
		}
		case 0x06: // LD B, d8
		{
			uint8_t n = ReadU8();
			B = n;
			CycleCount += 8;
			break;
		}
		case 0x32: // LD (HL-),A
		{
			WriteU8(HL, A);
			HL--;
			CycleCount += 8;
			break;
		}
		case 0x05: // DEC B
		{
			B--;
			ProgramCounter++;
			SET(StatusFlags, FLAG_N);
			UpdateHalfCarryFlag(B, -1);
			UpdateZeroFlag(B);
			CycleCount += 4;
			break;
		}
		case 0x20: // JR NZ, r8
		{
			if (TEST(StatusFlags, FLAG_Z)) // NZ
			{
				ProgramCounter++;
				ProgramCounter++;
				CycleCount += 8;
			}
			else
			{
				int8_t n = ReadI8();
				ProgramCounter += n;
				CycleCount += 12;
			}
			break;
		}
		case 0x0D: // DEC C
		{
			C--;
			ProgramCounter++;
			SET(StatusFlags, FLAG_N);
			UpdateHalfCarryFlag(C, -1);
			UpdateZeroFlag(C);
			CycleCount += 4;
			break;
		}
		default:
			assert(false);
		}
	}
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

void Reset()
{
	AF = 0x01B0;
	BC = 0x0013;
	DE = 0x00D8;
	HL = 0x014D;
	StackPointer = (uint16_t*) &RAM[0xFFFE];
	ProgramCounter = 0x100;
}

bool LoadROM()
{
	if (RAM)
	{
		VirtualFree(RAM, 0, MEM_RELEASE);
	}

	FILE* RomFile;
	int Filesize;
	fopen_s(&RomFile, "tetris.gb", "r");

	if (RomFile == NULL) {
		fprintf(stderr, "Can't open ROM file!\n");
		return false;
	}
	fseek(RomFile, 0, SEEK_END);
	Filesize = ftell(RomFile);
	fseek(RomFile, 0, SEEK_SET);

	RAM = (byte*)VirtualAlloc(0, UINT16_MAX, MEM_COMMIT, PAGE_READWRITE);
	if (RAM)
	{
		fread((byte*)RAM, sizeof(byte), Filesize, RomFile);
		fclose(RomFile);
	}

	Reset();

	return true;
}

void CleanUp()
{
	if (RAM)
	{
		VirtualFree(RAM, 0, MEM_RELEASE);
	}
	// probably move other cleanup to here as well
}


INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR lpCmdLine, INT nCmdShow)
{
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	uint32_t PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	if (!InitWindow(hInstance))
	{
		return 0;
	}

	ShowWindow(windowHandle, nCmdShow);

#if 0
	if (AllocConsole())
	{
		FILE* fi = 0;
		freopen_s(&fi, "CONOUT$", "w", stdout);
	}
#endif // DEBUG CONSOLE

	if (!LoadROM())
	{
		return 0;
	}

	char RomTitle[40];
	memcpy(RomTitle, &RAM[0x134], sizeof(RomTitle));
	char Title[40] = { "MinGB - " };
	strcat_s(Title, 40, RomTitle);
	SetWindowTextA(windowHandle, Title);

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

			//update();
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

