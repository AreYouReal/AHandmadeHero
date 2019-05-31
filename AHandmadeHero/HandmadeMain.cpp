#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>

// TODO: Implement sine ourselves
#include <math.h>

#define ever (;;) // Just for fun %)
#define Pi32 3.1459265359f

typedef float real32;
typedef double real64;

struct win32_offscreen_buffer {
	// Pixels are always 32 bits wide. Little endian 0x xx RR GG BB
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
static bool GlobalRunning;
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
		XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}

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
		GlobalRunning = false;
	}break;
	case WM_CLOSE: {
		// TODO: Handle with amessage to the user
		GlobalRunning = false;
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
				GlobalRunning = false;
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
		result = DefWindowProcA(Window, Message, wParam, lParam);
	}
			 break;
	}

	return(result);
}

struct win32_sound_output {
	int		SamplesPerSecond;
	int		ToneHz;
	int16_t ToneVolume;
	uint32_t RunningSampleIndex;
	int		WavePeriod;
	int		BytesPerSample;
	int		SecondaryBufferSize;
	real32	tSine;
	int		LatencySampleCount;
};

void
Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite ) {
	VOID* Region1;
	DWORD Region1Size;
	VOID* Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {

		// TODO: ASsert tha Region1Size is valid (And Region2Size as well)

		// TODO: Collapse these two loops
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		int16_t* SampleOut = (int16_t*)Region1;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex) {
			real32 SineValue = sinf(SoundOutput->tSine);
			int16_t SampleValue = (int16_t)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			SoundOutput->tSine += 2.0f * Pi32 * 1.0f / (real32)SoundOutput->WavePeriod;
			++SoundOutput->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		SampleOut = (int16_t*)Region2;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex) {
			real32 SineValue = sinf(SoundOutput->tSine);
			int16_t SampleValue = (int16_t)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;

			SoundOutput->tSine += 2.0f * Pi32 * 1.0f / (real32)SoundOutput->WavePeriod;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

int _stdcall
WinMain( HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR commandLine, int ShowCode) {
	
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	int64_t PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

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

			// NOTE: Sound test - Make this like sixty seconds ?
			win32_sound_output SoundOutput = {};
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.ToneHz = 256;
			SoundOutput.ToneVolume = 3000;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
			SoundOutput.BytesPerSample = sizeof(int16_t) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15.0f;

			Win32InitDSound(window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;
			
			LARGE_INTEGER LastCounter;
			QueryPerformanceCounter(&LastCounter);

			uint64_t LastCycleCount = __rdtsc();

			for ever{
				if (!GlobalRunning) { break; }

				MSG message;
				while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
					if (message.message == WM_QUIT) {
						GlobalRunning = false;
					}
					TranslateMessage(&message);
					DispatchMessage(&message);
				}

				// TODO: Should we pull this more frequently
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

						// TODO: We will do deadzone handling later using
						// XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
						// XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE

						/*if (AButton) {
							++yOffset;
						}*/
						////????
		/*				xOffset += StickX / 10000;
						yOffset += StickY / 10000;*/

						// TODO: Test it with XBox Controller
						/*SoundOutput.ToneHz = 512 + (int)(256.0f * ((real32)StickY / 30000.0f));
						SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;*/

					} else {
						// NOTE: The controller is not available
					}
				}

				WIN32RenderWeirdGradinent(&globalBackBuffer, xOffset, yOffset);

				// NOTE: Direct sound output test

				DWORD PlayCursor;
				DWORD WriteCursor;
				if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
					DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
					DWORD TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize;
					DWORD BytesToWrite;
					// TODO: Change this to using a lower latency offset from the playcursor
					// when we actually start having sound effects
					if (ByteToLock > TargetCursor) {
						BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
						BytesToWrite += TargetCursor;
					} else {
						BytesToWrite = TargetCursor - ByteToLock;
					}
					
					Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);

				}

				win32_window_dimension dimensions = GetWindowDimension(window);
				Win32DisaplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, globalBackBuffer);

				uint64_t EndCycleCount = __rdtsc();

				LARGE_INTEGER EndCounter;
				QueryPerformanceCounter(&EndCounter);



				// TODO: Display the value here
				uint64_t CyclesElapsed = EndCycleCount - LastCycleCount;

				int64_t CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
				float MSPerFrame = ((1000.0f * (float)CounterElapsed) / (float)PerfCountFrequency);
				float FPS = ((float)PerfCountFrequency / (float)CounterElapsed);
				float MCPF = (float)CyclesElapsed / 1000000.0f;


				char Buffer[256];
				sprintf_s(Buffer, " %.02fms/f,  %.02ff/s,  %.02fMc/f  \n", MSPerFrame, FPS, MCPF);
				OutputDebugStringA(Buffer);

				LastCounter = EndCounter;
				LastCycleCount = EndCycleCount;
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