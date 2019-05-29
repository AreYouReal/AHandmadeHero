#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

#include <math.h>

// Just for fun %)
#define ever (;;)
#define Pi32 3.1459265359f

typedef float real32;
typedef double real64;


struct win32_offscreen_buffer {
	// Pixels are always 32 bits wide. Littel endian 0x xx RR GG BB
	BITMAPINFO	info;
	void*		memory;
	int			width;
	int			height;
	int			pitch;
};

struct win32_window_dimension {
	int width;
	int height;
};

// TODO: This is a global for now
static bool globalRunning;
static win32_offscreen_buffer globalBackBuffer;
static LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// NOTE: XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
	return(0);
}
static x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE: XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
	return(ERROR_DEVICE_NOT_CONNECTED);
}
static x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

static void 
Win32LoadXInput() {
	// TODO: Test this on windows 8
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary) {
		XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}


	if (XInputLibrary) {
		XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
		if (XInputGetState) { XInputGetState = XInputGetStateStub; }
		XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
		if (XInputSetState) { XInputSetState = XInputSetStateStub; }
	} else {
		// TODO: Diagnostic
	}
}

static void 
Win32InitDSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize) {
	// NOTE: Load the library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	if (DSoundLibrary) {
		// NOTE: Get a DirectSound object
		direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED( DirectSoundCreate(0, &DirectSound, 0) ) ) {

			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = WaveFormat.nChannels * WaveFormat.wBitsPerSample / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;

			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
				

				//STDMETHOD(CreateSoundBuffer)    (THIS_ _In_ LPCDSBUFFERDESC pcDSBufferDesc, _Outptr_ LPDIRECTSOUNDBUFFER * ppDSBuffer, _Pre_null_ LPUNKNOWN pUnkOuter) PURE;

				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// NOTE: "Create" a primary buffer
				// TODO: DSBCAPS_GLOBALFOCUS
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0);

				if (SUCCEEDED(Error)) {
					if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))) {
						OutputDebugStringA("Primary buffer format was set. \n");
					} else {
						// TODO: Diagnostic
					}
				} else {
					// TODO: Diagnostic
				}
			} else {
				// NOTE : Diagnostic
			}
			
			// NOTE: "Create" a secondary buffer
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			
			HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
			if (SUCCEEDED(Error)) {
				OutputDebugStringA("Secondary buffer created successfully. \n");
				// NOTE: Start it playing		
			}
	
		} else {
			// TODO: Diagnostic
		}
	} else {
		// TODO: Diagnostic
	}




}

static win32_window_dimension
GetWindowDimension(HWND window) {

	win32_window_dimension result;

	RECT clientRect;
	GetClientRect(window, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;

	return(result);
}

static void
WIN32RenderWeirdGradinent(win32_offscreen_buffer* buffer, int xOffset, int yOffset) {

	// TODO: Let's see waht the optimizer does
	uint8_t* row = (uint8_t*)buffer->memory;
	for (int y = 0; y < buffer->height; ++y) {
		uint32_t* pixel = (uint32_t*)row;
		for (int x = 0; x < buffer->width; ++x) {
			/*
				Pixel in memory: RR GG BB xx
				LITTLE ENDIAN: 0x xxBBGGRR
				BIG ENDIAN: 0x RRGGBBxx
			*/
			uint8_t blue = x + xOffset;
			uint8_t green = y + yOffset;
			/*
				Memory: 	BB GG RR xx
				Register:	xx RR GG BB (little endian)
				Pixel (32-bits)
			*/

			* pixel++ = (green << 8 | blue);
		}
		row += buffer->pitch;
	}
}

static void
Win32ResizeDIBSection(win32_offscreen_buffer * buffer, int width, int height) {

	if (buffer->memory) {
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}

	buffer->width = width;
	buffer->height = height;
	int bytesPerPixel = 4;

	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = buffer->width;
	buffer->info.bmiHeader.biHeight = -buffer->height; // Top to down Y (if + -> bottom to top Y value)
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;

	int bitmapMemorySize = bytesPerPixel * buffer->width * buffer->height;
	buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	buffer->pitch = buffer->width * bytesPerPixel;
	// TODO: Probably clear this to black
}

static void
Win32DisaplayBufferInWindow(HDC deviceContext, int windowWidth, int windowHeight, win32_offscreen_buffer buffer ) {
	// TODO: Aspect ration correction
	// TODO: Play with stretches
	StretchDIBits(deviceContext,
		0, 0, windowWidth, windowHeight,
		0, 0, buffer.width, buffer.height,
		buffer.memory,
		&buffer.info,
		DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam) {

	LRESULT result = 0;
	switch (Message) {
	case WM_ACTIVATEAPP: {
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	}break;
	case WM_DESTROY: {
		// TODO: Handle this with an error
		globalRunning = false;
	}break;
	case WM_CLOSE: {
		// TODO: Handle with amessage to the user
		globalRunning = false;
	}break;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP: {

		uint32_t VKCode = wParam;

		bool WasDown = ((lParam & (1 << 30)) != 0);
		bool IsDown = ((lParam & (1 << 31)) == 0);

		if (WasDown != IsDown) {
			if (VKCode == 'W') {
				OutputDebugStringA("W ");
			} else if (VKCode == 'A') {

			} else if (VKCode == 'S') {

			} else if (VKCode == 'D') {

			} else if (VKCode == 'Q') {

			} else if (VKCode == 'E') {

			} else if (VKCode == VK_UP) {

			} else if (VKCode == VK_LEFT) {

			} else if (VKCode == VK_DOWN) {

			} else if (VKCode == VK_RIGHT) {

			} else if (VKCode == VK_ESCAPE) {
				OutputDebugStringA("ESCAPE: ");
				if (IsDown) {
					OutputDebugStringA("isDown \n");
				}
				if (WasDown) {
					OutputDebugStringA("WasDown\n");
				}
			} else if (VKCode == VK_SPACE) {

			}
			bool AltKeyWasDown = ( (lParam & (1 << 29)) != 0); 
			if (VKCode == VK_F4 && AltKeyWasDown) {
				globalRunning = false;
			}
		}

		
		
		
	}break;

	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC deviceContext = BeginPaint(Window, &paint);

		int x = paint.rcPaint.left;
		int y = paint.rcPaint.bottom;
		int height = paint.rcPaint.bottom - paint.rcPaint.top;
		int width = paint.rcPaint.right - paint.rcPaint.left;

		win32_window_dimension dimensions = GetWindowDimension(Window);

		Win32DisaplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, globalBackBuffer);

		EndPaint(Window, &paint);
	}break;
	default: {
		// OutputDebugStringA("default\n");
		result = DefWindowProc(Window, Message, wParam, lParam);
	}
			 break;
	}

	return(result);
}

int _stdcall
WinMain( HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR commandLine, int ShowCode) {
	Win32LoadXInput();

	WNDCLASSA windowClass = {};

	Win32ResizeDIBSection(&globalBackBuffer, 1280, 720);

	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = Win32MainWindowCallback;
	windowClass.hInstance = Instance;
	windowClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClass(&windowClass)) {
		HWND window =
			CreateWindowEx(
				0,
				windowClass.lpszClassName,
				"A Handmade Hero",
				WS_OVERLAPPEDWINDOW | WS_VISIBLE,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				0,
				0,
				Instance,
				0
			);
		if (window) {

			HDC deviceContext = GetDC(window);

			// NOTE: Graphics test
			int xOffset = 0;
			int yOffset = 0;

			// NOTE: Sound test
			int SamplesPerSecond = 48000;
			int ToneHz = 256;
			int16_t ToneVolume = 5000;
			uint32_t RunningSampleIndex = 0;
			int WavePeriod = SamplesPerSecond / ToneHz;
			//int HalfWavePeriod = WavePeriod / 2;
			int BytesPerSample = sizeof(int16_t) * 2;
			int SecondaryBufferSize = SamplesPerSecond * BytesPerSample;


			Win32InitDSound(window, SamplesPerSecond, SecondaryBufferSize);
			bool SoundIsPlaying = false;

			globalRunning = true;
			for ever{
				if (!globalRunning) {
					break;
				}
				MSG message;
				while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
					if (message.message == WM_QUIT) {
						globalRunning = false;
					}
					TranslateMessage(&message);
					DispatchMessage(&message);
				}

				// TODO: Shoudl we pull this more frequently
				for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex) {
					XINPUT_STATE controllerState;
					if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS) {
						// NOTE: This Controller is plugged in
						// TODO: See if ControllerState.dwPacketNUmber increments too rapidly
						XINPUT_GAMEPAD* Pad = &controllerState.Gamepad;
						bool Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
						bool Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
						bool Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
						bool Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
						bool Start = Pad->wButtons & XINPUT_GAMEPAD_START;
						bool Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;

						bool LeftShoulder = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
						bool RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
						bool AButton = Pad->wButtons & XINPUT_GAMEPAD_A;
						bool BButton = Pad->wButtons & XINPUT_GAMEPAD_B;
						bool XButton = Pad->wButtons & XINPUT_GAMEPAD_X;
						bool YButton = Pad->wButtons & XINPUT_GAMEPAD_Y;

						int16_t StickX = Pad->sThumbLX;
						int16_t StickY = Pad->sThumbLY;

						/*if (AButton) {
							++yOffset;
						}*/
						//????
						/*xOffset += StickX >> 15;
						yOffset += StickY >> 15;
*/

					} else {
						// NOTE: The controller is not available
					}
				}

				WIN32RenderWeirdGradinent(&globalBackBuffer, xOffset, yOffset);

				// NOTE: Direct sound output test

				DWORD PlayCursor;
				DWORD WriteCursor;

				if (!SoundIsPlaying && SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {


					DWORD ByteToLock = RunningSampleIndex * BytesPerSample % SecondaryBufferSize;
					DWORD BytesToWrite;
					if (ByteToLock == PlayCursor) {
						BytesToWrite = SecondaryBufferSize;
					}
					// TODO: We need a more accurate check than BytetoLock == PlayCursor
					if (ByteToLock > PlayCursor) {
						BytesToWrite = SecondaryBufferSize - ByteToLock;
						BytesToWrite += PlayCursor;
					} else {
						BytesToWrite = PlayCursor - ByteToLock;
					}
					
					VOID* Region1;
					DWORD Region1Size;
					VOID* Region2;
					DWORD Region2Size;

					if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {

						// TODO: ASsert tha Region1Size is valid (And Region2Size as well)
						
						// TODO: Collapse these two loops
						DWORD Region1SampleCount = Region1Size / BytesPerSample;
						int16_t* SampleOut = (int16_t*)Region1;
						for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex) {
							// TODO: Draw this out for people
							real32 t = 2.0f * Pi32 * (real32)RunningSampleIndex / (real32)WavePeriod;
							real32 SineValue = sinf(t);
							int16_t SampleValue = (int16_t)(SineValue * ToneVolume) ;
							*SampleOut++ = SampleValue;
							*SampleOut++ = SampleValue;
							++RunningSampleIndex;
						}
						
						DWORD Region2SampleCount = Region2Size / BytesPerSample;
						SampleOut = (int16_t*)Region2;
						for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex) {
							real32 t = 2.0f * Pi32 * (real32)RunningSampleIndex / (real32)WavePeriod;
							real32 SineValue = sinf(t);
							int16_t SampleValue = (int16_t)(SineValue * ToneVolume);
							*SampleOut++ = SampleValue;
							*SampleOut++ = SampleValue;
							++RunningSampleIndex;
						}

						GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
					}


				}

				if (!SoundIsPlaying) {
					GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
					SoundIsPlaying = true;
				}

				win32_window_dimension dimensions = GetWindowDimension(window);
				Win32DisaplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, globalBackBuffer);

				//}

			}
		} else {
			// TODO: Logging
		}
	}
	else {
		// TODO: Logging
	}

	return 0;
}