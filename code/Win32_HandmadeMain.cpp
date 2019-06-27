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
static int64_t GlobalPerfCountFrequency;

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

static debug_read_file_result
DEBUGPlatformReadEntireFile(char* Filename){
	debug_read_file_result Result = {};

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
	if(FileHandle != INVALID_HANDLE_VALUE){
		LARGE_INTEGER FileSize;
		if(GetFileSizeEx(FileHandle, &FileSize)){
			uint32_t FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents =  VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if(Result.Contents){
				DWORD BytesRead;
				if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead)) {
					// NORE: File read success
					Result.ContentsSize = BytesRead;

				}else{
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}else{
				// TODO: Logging
			}

		}else{
			// TODO: Logging
		}
		CloseHandle(FileHandle);
	}else{
		// TODO: Logging
	}
	return(Result);
}

static void 
DEBUGPlatformFreeFileMemory(void *Memory){
	if (Memory) {
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

static bool 
DEBUGPlatformWriteEntireFile(char* Filename, uint32_t MemorySize, void* Memory){
	bool Result = false;

	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if(FileHandle != INVALID_HANDLE_VALUE){
		DWORD BytesWritten;
		if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0)){
			Result = (MemorySize == BytesWritten);
		}else{
			// TODO: Logging
		}
		CloseHandle(FileHandle);
	}else{
		// TODO: Logging
	}

	return(Result);
}


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

		Assert(!"Keyboard event came in through a non-dispatch message!");

		uint32_t VKCode = (uint32_t)wParam;

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
Win32ProcessKeyboardMessage(game_button_state* NewState, bool IsDown) {
	Assert(NewState->EndedDown != IsDown);
	NewState->EndedDown = IsDown;
	++NewState->HalfTransitionCount;
}

static void 
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state* OldState, DWORD ButtonBit, game_button_state* Newstate) {
	Newstate->EndedDown = (( XInputButtonState & ButtonBit) == ButtonBit);
	Newstate->HalfTransitionCount = (OldState->EndedDown != Newstate->EndedDown) ? 1 : 0;
}

static float
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold){
	return(Value < - DeadZoneThreshold ? (float)(Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold) : (Value > DeadZoneThreshold ? (float)(Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold) : 0));
}

static void 
Win32ProcessPendingMessages(game_controller_input* KeyboardController){

	MSG Message;

	while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)){

		switch (Message.message){
			case WM_QUIT:
			GlobalRunning = false;
			break;

			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				uint32_t VKCode = (uint32_t)Message.wParam;

				bool WasDown = ((Message.lParam & (1 << 30)) != 0);
				bool IsDown = ((Message.lParam & (1 << 31)) == 0);

				if (WasDown != IsDown)
				{
					if (VKCode == 'W')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
					}
					else if (VKCode == 'A')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
					}
					else if (VKCode == 'S')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
					}
					else if (VKCode == 'D')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
					}
					else if (VKCode == 'Q')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
					}
					else if (VKCode == 'E')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
					}
					else if (VKCode == VK_UP)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
					}
					else if (VKCode == VK_LEFT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
					}
					else if (VKCode == VK_DOWN)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
					}
					else if (VKCode == VK_RIGHT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
					}
					else if (VKCode == VK_ESCAPE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
					}
					else if (VKCode == VK_SPACE)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
					}
					bool AltKeyWasDown = ((Message.lParam & (1 << 29)) != 0);
					if (VKCode == VK_F4 && AltKeyWasDown)
					{
						GlobalRunning = false;
					}
				}
			}
			break;

			default:
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			break;
		}
	}
}


inline LARGE_INTEGER 
Win32GetWallClock(){
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return(Result);
}

inline float
GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End){
	return((float)(End.QuadPart - Start.QuadPart) / (float)GlobalPerfCountFrequency);
}

int _stdcall
WinMain( HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR commandLine, int ShowCode) {
	
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	// NOTE: Set the Windows scheduler granularity to 1 ms
	// so our Sleep() can be more granular
	UINT DesiredSchedulerMS = 1;
	bool SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
	Win32LoadXInput();

	WNDCLASSA windowClass = {};

	Win32ResizeDIBSection(&globalBackBuffer, 1280, 720);

	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = Win32MainWindowCallback;
	windowClass.hInstance = Instance;
	windowClass.lpszClassName = "HandmadeHeroWindowClass";

	int MonitorRefreshHz = 60;
	int GameUpdateHz = MonitorRefreshHz / 2;
	float TargetSecondsElapsedPerFrame = 1.0f / MonitorRefreshHz;

	// TODO: How do we reliably query on this on Windows?
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
			SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;

			Win32InitDSound(window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;
			
			// TODO: Pool with bitmap VirtualAlloc
			int16_t* Samples = (int16_t*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
			LPVOID BaseAddress = 0;
#endif

			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes(4);

			// TODO: Handle various memory footprint (Using System Metrics)
			uint64_t TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, (size_t)TotalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			GameMemory.TransientStorage = ((uint8_t*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

			if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage){

				game_input Input[2];
				game_input* NewInput = &Input[0];
				game_input* OldInput = &Input[1];
				game_input ZeroInput = {};
				*NewInput = *OldInput = ZeroInput;

				LARGE_INTEGER LastCounter = Win32GetWallClock();

				uint64_t LastCycleCount = __rdtsc();
				for ever{
					if (!GlobalRunning) { break; }

					game_controller_input* OldKeyboardController = GetController(OldInput, 0);
					game_controller_input* NewKeyboardController = GetController(NewInput, 0);
					// TODO: Zeroing macro
					// TODO: We can't zero everything because the up/down state will be wrong!
					*NewKeyboardController = {};
					NewKeyboardController->IsConnected = true;
					for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex){
						NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}

					Win32ProcessPendingMessages(NewKeyboardController);
					
					// TODO: Need to not pool disconnected controllers to avoid 
					// xinput frame rate hit on older libraries...
					// TODO: Should we pull this more frequently
					DWORD MaxControllerCount = XUSER_MAX_COUNT;
					if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1)) {
						MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
					}
					for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ++ControllerIndex) {

						DWORD OurControllerIndex = ControllerIndex + 1;
						game_controller_input* OldController = GetController(OldInput, OurControllerIndex);
						game_controller_input* NewController = GetController(NewInput, OurControllerIndex);

						XINPUT_STATE controllerState;
						if (XInputGetState(ControllerIndex, &controllerState) == ERROR_SUCCESS) {
							NewController->IsConnected = true;
							// NOTE: This Controller is plugged in
							// TODO: See if ControllerState.dwPacketNUmber increments too rapidly
							XINPUT_GAMEPAD* Pad = &controllerState.Gamepad;

							// TODO: This is a square deadzone, check XInput to
							// verify that the deadzone is "round" and show how to do
							// round deadzone processing.
							NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
							NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

							if(NewController->StickAverageX != 0.0f || NewController->StickAverageY != 0.0f){
								NewController->IsAnalog = true;
							}

							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP){ 
								NewController->StickAverageY = 1.0f; 
								NewController->IsAnalog = false;	
							}

							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN){ 
								NewController->StickAverageY = -1.0f; 
								NewController->IsAnalog = false;	
							}
							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT){ 
								NewController->StickAverageX = -1.0f; 
								NewController->IsAnalog = false;
							}
							if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT){ 
								NewController->StickAverageX = 1.0f; 
								NewController->IsAnalog = false;
							}

							float Threshold = 0.5f;
							Win32ProcessXInputDigitalButton(NewController->StickAverageX > Threshold ? 1 : 0, &OldController->MoveRight, 1, &NewController->MoveRight);
							Win32ProcessXInputDigitalButton(NewController->StickAverageX < -Threshold ? 1 : 0, &OldController->MoveLeft, 1, &NewController->MoveLeft);
							Win32ProcessXInputDigitalButton(NewController->StickAverageY > Threshold ? 1 : 0, &OldController->MoveUp, 1, &NewController->MoveUp);
							Win32ProcessXInputDigitalButton(NewController->StickAverageY < -Threshold ? 1 : 0, &OldController->MoveDown, 1, &NewController->MoveDown);

							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown, XINPUT_GAMEPAD_A, &NewController->ActionDown);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight, XINPUT_GAMEPAD_B, &NewController->ActionRight);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft, XINPUT_GAMEPAD_X, &NewController->ActionLeft);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp, XINPUT_GAMEPAD_Y, &NewController->ActionUp);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start);
							Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back);

						} else {
							NewController->IsConnected = false;
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
					GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);
					//WIN32RenderWeirdGradinent(&globalBackBuffer, xOffset, yOffset);

					// NOTE: Direct sound output test

					if (SoundIsValid) {
						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}

					LARGE_INTEGER WorkCounter = Win32GetWallClock();
					float WorkSecondsElapsed = GetSecondsElapsed(LastCounter, WorkCounter);
					
					// TODO: Not tested yet! Probably buggy!!!!
					float SecondsElapsedForFrame = WorkSecondsElapsed;
					if(SecondsElapsedForFrame < TargetSecondsElapsedPerFrame){
						while(SecondsElapsedForFrame < TargetSecondsElapsedPerFrame){
							// if(SleepIsGranular){
							// 	DWORD SleepMS = (DWORD)((TargetSecondsElapsedPerFrame - SecondsElapsedForFrame) * 1000.0f);
							// 	Sleep(SleepMS);
							// }
							SecondsElapsedForFrame = GetSecondsElapsed(LastCounter, Win32GetWallClock());
						}
					}else{
						// TODO: MISSED FRAME RATE!
						// TODO: Logging


					}

					win32_window_dimension dimensions = GetWindowDimension(window);
					Win32DisaplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, globalBackBuffer);

					game_input* Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;

					uint64_t EndCycleCount = __rdtsc();
					uint64_t CyclesElapsed = EndCycleCount - LastCycleCount;
					LastCycleCount = EndCycleCount;

					LARGE_INTEGER EndCounter = Win32GetWallClock(); 
					
					float MSPerFrame = 1000.0f * GetSecondsElapsed(LastCounter, EndCounter);
					LastCounter = EndCounter;

					float FPS = 0.0f;
					float MCPF = (float)CyclesElapsed / 1000000.0f;
					char FPSBuffer[256];
					sprintf_s(FPSBuffer, " %.02fms/f,  %.02ff/s,  %.02fMc/f  \n", MSPerFrame, FPS, MCPF);
					OutputDebugStringA(FPSBuffer);



					// TODO: Shpuld i clear this here?
				}

			}else{
				// TODO: Logging
			}

		} else {
			// TODO: Logging
		}
	} else {
		// TODO: Logging
	}

	return 0;
}