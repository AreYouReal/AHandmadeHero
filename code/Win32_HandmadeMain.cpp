/*
	TODO: THIS IS NOT A FINAL PLATFORM LAYER!!!

	- Saved game location
	- GEtting a handle to our own executable file
	- Asset loading path
	- Threading (launch a thread)
	- Raw Input (support for multiple keyboards)
	- Sleep/rimeBeginPeriod
	- ClipCursor() (for multimonitor support)
	- Fullscreen support
	- WM_SETCURSOR (control cursor visibility)
	- WM_ACTIVATEAPP (for when we are not active application)
	- Blit speed improvements (BitBlt)
	- Hardware acceleration (OpenGL or Direct3D or BOTH???)
	- GetKEyboardLayout (for French keyboard, international WASD support)

	Just a partial list of stuf!!!
		
*/

// TODO: Implement sine ourselves
#include <math.h>

#define ever (;;) // Just for fun %)
#define Pi32 3.1459265359f

typedef float real32;
typedef double real64;

// Handmade headers	
#include "AHandmade.h"
#include "AHandmade.cpp"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "Win32_Handmade.h"

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
		if (!XInputGetState) { XInputGetState = XInputGetStateStub; }
		XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
		if (!XInputSetState) { XInputSetState = XInputSetStateStub; }
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

static void
Win32ClearBuffer(win32_sound_output* SoundOutput) {
	VOID* Region1;
	DWORD Region1Size;
	VOID* Region2;
	DWORD Region2Size;
	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {
		int8_t* DestSample = (int8_t*)Region1;
		for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex) { *DestSample++ = 0; }

		DestSample = (int8_t*)Region2;
		for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex) { *DestSample++ = 0; }
	}

	GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
}

static void
Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer* SourceBuffer ) {
	VOID* Region1;
	DWORD Region1Size;
	VOID* Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0))) {

		// TODO: ASsert tha Region1Size is valid (And Region2Size as well)

		// TODO: Collapse these two loops
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		int16_t* DestSample = (int16_t*)Region1;
		int16_t* SourceSample = SourceBuffer->Samples;

		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex) {
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		DestSample = (int16_t*)Region2;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex) {
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

static void 
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state* OldState, DWORD ButtonBit, game_button_state* Newstate) {
	Newstate->EndedDown = (( XInputButtonState & ButtonBit) == ButtonBit);
	Newstate->HalfTransitionCount = (OldState->EndedDown != Newstate->EndedDown) ? 1 : 0;
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

			// NOTE: Sound test - Make this like sixty seconds ?
			win32_sound_output SoundOutput = {};
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.BytesPerSample = sizeof(int16_t) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15.0f;

			Win32InitDSound(window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;
			
			// TODO: Pool with bitmap VirtualAlloc
			int16_t* Samples = (int16_t*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);


			game_input Input[2];
			game_input* NewInput = &Input[0];
			game_input* OldInput = &Input[1];

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
				int MaxControllerCount = XUSER_MAX_COUNT;
				if (MaxControllerCount > ArrayCount(NewInput->Controllers)) {
					MaxControllerCount = ArrayCount(NewInput->Controllers);
				}
				for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex) {


					game_controller_input* OldController = &OldInput->Controllers[ControllerIndex];
					game_controller_input* NewController = &NewInput->Controllers[ControllerIndex];

					XINPUT_STATE controllerState;
					if (XInputGetState(ControllerIndex, &controllerState) == ERROR_SUCCESS) {
						// NOTE: This Controller is plugged in
						// TODO: See if ControllerState.dwPacketNUmber increments too rapidly
						XINPUT_GAMEPAD* Pad = &controllerState.Gamepad;
						
						// TODO: DPad
						bool Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
						bool Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
						bool Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
						bool Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

						NewController->IsAnalog = true;
						NewController->StartX = OldController->EndX;
						NewController->StartY = OldController->EndY;

						// TODO: We will do deadzone handling later using
						// XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
						// XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE
						// TODO: Min/Max macros???
						float X;
						if (Pad->sThumbLX < 0) {
							X = (float)Pad->sThumbLX / 32768.0f;
						} else {
							X = (float)Pad->sThumbLX / 32767.0f;
						}
						NewController->MinX = NewController->MaxX = NewController->EndX = X;


						float Y;
						if (Pad->sThumbLY < 0) {
							Y = (float)Pad->sThumbLY / 32768.0f;
						} else {
							Y = (float)Pad->sThumbLY / 32767.0f;
						}
						NewController->MinY = NewController->MaxY = NewController->EndY = Y;
						

						Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Down, XINPUT_GAMEPAD_A, &NewController->Down);
						Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Right, XINPUT_GAMEPAD_B, &NewController->Right);
						Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Left, XINPUT_GAMEPAD_X, &NewController->Left);
						Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Up, XINPUT_GAMEPAD_Y, &NewController->Up);
						Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
						Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);

						/*bool Start = Pad->wButtons & XINPUT_GAMEPAD_START;
						bool Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;
*/

					} else {
						// NOTE: The controller is not available
					}
				}


				DWORD PlayCursor = 0;
				DWORD WriteCursor = 0;
				DWORD ByteToLock = 0;
				DWORD TargetCursor = 0;
				DWORD BytesToWrite = 0;
				bool SoundIsValid = false;
				// TODO: Tighten sound logic so that we know where we should be 
				// writing to and can anticipate the time spent in the game update
				if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
					ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
					TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize;
					if (ByteToLock > TargetCursor) {
						BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
						BytesToWrite += TargetCursor;
					}
					else {
						BytesToWrite = TargetCursor - ByteToLock;
					}
					SoundIsValid = true;
				}

				game_sound_output_buffer SoundBuffer = {};
				SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
				SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
				SoundBuffer.Samples = Samples;

				game_offscreen_buffer Buffer = {};
				Buffer.memory = globalBackBuffer.memory;
				Buffer.width = globalBackBuffer.width;
				Buffer.height = globalBackBuffer.height;
				Buffer.pitch = globalBackBuffer.pitch;
				GameUpdateAndRender(NewInput, &Buffer, &SoundBuffer);
				//WIN32RenderWeirdGradinent(&globalBackBuffer, xOffset, yOffset);

				// NOTE: Direct sound output test

				if (SoundIsValid) {
					Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
				}

				win32_window_dimension dimensions = GetWindowDimension(window);
				Win32DisaplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, globalBackBuffer);

				uint64_t EndCycleCount = __rdtsc();

				LARGE_INTEGER EndCounter;
				QueryPerformanceCounter(&EndCounter);


				uint64_t CyclesElapsed = EndCycleCount - LastCycleCount;
				int64_t CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
				float MSPerFrame = ((1000.0f * (float)CounterElapsed) / (float)PerfCountFrequency);
				float FPS = ((float)PerfCountFrequency / (float)CounterElapsed);
				float MCPF = (float)CyclesElapsed / 1000000.0f;


				//char Buffer[256];
				//sprintf_s(Buffer, " %.02fms/f,  %.02ff/s,  %.02fMc/f  \n", MSPerFrame, FPS, MCPF);
				//OutputDebugStringA(Buffer);

				LastCounter = EndCounter;
				LastCycleCount = EndCycleCount;

				game_input* Temp = NewInput;
				NewInput = OldInput;
				OldInput = Temp;
				// TODO: Shpuld i clear this here?
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