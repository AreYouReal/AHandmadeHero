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
static bool GlobalPause;
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
			BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
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
	buffer->BytesPerPixel = bytesPerPixel;

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
Win32DisplayBufferInWindow(HDC deviceContext, int windowWidth, int windowHeight, win32_offscreen_buffer buffer ) {
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
		Win32DisplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, globalBackBuffer);
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
#if HANDMADE_INTERNAL
					else if (VKCode == 'P') {
						if (IsDown) {
							GlobalPause = !GlobalPause;
						}
					}
#endif
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
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End){
	return((float)(End.QuadPart - Start.QuadPart) / (float)GlobalPerfCountFrequency);
}

static void 
Win32DebugDrawVertical(win32_offscreen_buffer* BackBuffer, int X, int Top, int Bottom, uint32_t Color){
	if (Top <= 0) {
		Top = 0;
	}

	if (Bottom > BackBuffer->height) {
		Bottom = BackBuffer->height;
	}

	if (X >= 0 && X < BackBuffer->width) {
		uint8_t* Pixel = (uint8_t*)BackBuffer->memory + X * BackBuffer->BytesPerPixel + Top * BackBuffer->pitch;
		for (int y = Top; y < Bottom; ++y) {
			*(uint32_t*)Pixel = Color;
			Pixel += BackBuffer->pitch;
		}
	}
}

inline void 
Win32DrawSoundBufferMarker(win32_offscreen_buffer* BackBuffer, win32_sound_output* SoundOutput, float C, int PadX, int Top, int Bottom, DWORD Value, uint32_t Color){
	float XFloat = (C * (float)Value);
	int X = PadX + (int)XFloat;
	Win32DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

static void
Win32DebugSyncDisplay(win32_offscreen_buffer* BackBuffer, int MarkerCount, win32_debug_time_marker* Markers, int CurrentMarkerIndex, win32_sound_output* SoundOutput, float TargetSecondsPerFrame){
	// TODO: Draw where we are writing out sound
	int PadX = 16;
	int PadY = 16;
	
	int LineHeight = 64;


	float C = (float)(BackBuffer->width - 2 * PadX) / (float)SoundOutput->SecondaryBufferSize;
	for(int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex){
		win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
		Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
		Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);

		DWORD PlayColor = 0xFFFFFFFF;
		DWORD WriteColor = 0xFFFF0000;
		DWORD ExpectedFlipColor = 0xFFFFFF00;
		DWORD PlayWindowColor = 0xFFFF00FF;

		int Top = PadY; 
		int Bottom = PadY + LineHeight; 
		if(MarkerIndex == CurrentMarkerIndex){
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

			int FirstTop = Top;

			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);
	

			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;
			
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);
		
			Top += LineHeight + PadY;
			Bottom += LineHeight + PadY;

			Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipCursor, ExpectedFlipColor);
		}


		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480 * SoundOutput->BytesPerSample, PlayWindowColor);
		Win32DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
	}
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

// TODO: Let's think about running non-frame-quantized  for audio latency...
// TODO: Let's use the write cursor delta from the play cursor to adjust
// the target audio latency. 
#define MonitorRefreshHz 60
#define GameUpdateHz (MonitorRefreshHz / 2)
	float TargetSecondsPerFrame = 1.0f / MonitorRefreshHz;

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
			// TODO: Get rid of LatencySampleCount
			SoundOutput.LatencySampleCount = 3*(SoundOutput.SamplesPerSecond / GameUpdateHz);
			// TODO: Actually compute this variance and see
			// what the lowest reasonable value is
			SoundOutput.SafetyBytes =  (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample / GameUpdateHz) / 3;
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
				LARGE_INTEGER FlipWallClock = Win32GetWallClock();

				int DebugTimeMarkerIndex = 0;
				win32_debug_time_marker DebugTimeMarkers[GameUpdateHz] = {0};

				DWORD AudioLatencyBytes = 0;
				float AudioLatencySeconds = 0.0f;
				bool SoundIsValid = false;
				
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
					if (!GlobalPause) {
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





						game_offscreen_buffer Buffer = {};
						Buffer.memory = globalBackBuffer.memory;
						Buffer.width = globalBackBuffer.width;
						Buffer.height = globalBackBuffer.height;
						Buffer.pitch = globalBackBuffer.pitch;
						GameUpdateAndRender(&GameMemory, NewInput, &Buffer);

						LARGE_INTEGER AudioWallClock = Win32GetWallClock();
						float FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

						DWORD PlayCursor;
						DWORD WriteCursor;
						if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)) {
							/* NOTE:

								Here is how sound output computation works.

								We define a safety value that is the number of the samples
								we think our game update loop may vary by (let's say up to 2ms)

								When we wake up to write audio, we will look and see what the play
								cursor position is and we will forecast ahead where we think the
								play cursor will be on the next frame boundary.

								We will the look to see if the write cursor is before that by at least our safety value.
								If it is, we will write up to the next frame boundary from the
								write cursor, and then one frame further, giving us perfect
								audio sync in the case of a card that has low enough  latency.

								If the write cursor is _after_ that safety margin,
								then we assume we can never sync the audio perfectly,
								so we will write one frame's worth of audio plus
								the safety margin's worth of guard samples.
							*/

							if (!SoundIsValid) {
								SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
								SoundIsValid = true;
							}

							DWORD ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize);

							DWORD ExpectedSoundBytesPerFrame = (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz;
							float SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
							DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) * (float)ExpectedSoundBytesPerFrame);
							DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;


							DWORD SafeWriteCursor = WriteCursor;
							if (SafeWriteCursor < PlayCursor) {
								SafeWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							Assert(SafeWriteCursor >= PlayCursor);
							SafeWriteCursor += SoundOutput.SafetyBytes;
							bool bAudioCardIsLowLatency = SafeWriteCursor < ExpectedFrameBoundaryByte;

							DWORD TargetCursor = 0;
							if (bAudioCardIsLowLatency) {
								TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
							}
							else {
								TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
							}
							TargetCursor %= SoundOutput.SecondaryBufferSize;

							DWORD BytesToWrite = 0;
							if (ByteToLock > TargetCursor) {
								BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
								BytesToWrite += TargetCursor;
							}
							else {
								BytesToWrite = TargetCursor - ByteToLock;
							}


							game_sound_output_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
							SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
							SoundBuffer.Samples = Samples;
							GameGetSoundSamples(&GameMemory, &SoundBuffer);


							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);

#if HANDMADE_INTERNAL
							win32_debug_time_marker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
							Marker->OutputPlayCursor = PlayCursor;
							Marker->OutputWriteCursor = WriteCursor;
							Marker->OutputLocation = ByteToLock;
							Marker->OutputByteCount = BytesToWrite;
							Marker->ExpectedFlipCursor = ExpectedFrameBoundaryByte;

							DWORD UnwrappedWriteCursor = WriteCursor;
							if (UnwrappedWriteCursor < PlayCursor) {
								UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
							}

							AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
							AudioLatencySeconds = ((float)AudioLatencyBytes / (float)SoundOutput.BytesPerSample) / (float)SoundOutput.SamplesPerSecond;
							char TextBuffer[256];
							_snprintf_s(TextBuffer, sizeof(TextBuffer), "BTL: %u TC: %u BTW: %u PC: %u WC: %u DELTA:%u (%fs)\n",
								ByteToLock, TargetCursor, BytesToWrite, PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
							OutputDebugStringA(TextBuffer);
#endif
						}
						else {
							SoundIsValid = false;
						}

						LARGE_INTEGER WorkCounter = Win32GetWallClock();
						float WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

						// TODO: Not tested yet! Probably buggy!!!!
						float SecondsElapsedForFrame = WorkSecondsElapsed;
						if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
							if (SleepIsGranular) {
								DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
								if (SleepMS > 0) {
									Sleep(SleepMS);
								}
							}
							float TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());

							if (TestSecondsElapsedForFrame < TargetSecondsPerFrame) {
								// TODO: LOG MISSED SLEEP HERE
							}

							while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							}
						}
						else {
							// TODO: MISSED FRAME RATE!
							// TODO: Logging


						}

						LARGE_INTEGER EndCounter = Win32GetWallClock();
						float MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;

						win32_window_dimension dimensions = GetWindowDimension(window);
#if HANDMADE_INTERNAL
						// TODO: Note, current is wrong on the zero'th index
						Win32DebugSyncDisplay(&globalBackBuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers, DebugTimeMarkerIndex - 1, &SoundOutput, TargetSecondsPerFrame);
#endif

						Win32DisplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, globalBackBuffer);


						FlipWallClock = Win32GetWallClock();

#if HANDMADE_INTERNAL
						// NOTE: This is debug code
						{
							if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor))) {
								Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
								win32_debug_time_marker* Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
								Marker->FlipPlayCursor = PlayCursor;
								Marker->FlipWriteCursor = WriteCursor;
							}


						}
#endif

						game_input* Temp = NewInput;
						NewInput = OldInput;
						OldInput = Temp;

						uint64_t EndCycleCount = __rdtsc();
						uint64_t CyclesElapsed = EndCycleCount - LastCycleCount;
						LastCycleCount = EndCycleCount;

						float FPS = 0.0f;
						float MCPF = (float)CyclesElapsed / 1000000.0f;
						char FPSBuffer[256];
						sprintf_s(FPSBuffer, " %.02fms/f,  %.02ff/s,  %.02fMc/f  \n", MSPerFrame, FPS, MCPF);
						OutputDebugStringA(FPSBuffer);

#if HANDMADE_INTERNAL
						++DebugTimeMarkerIndex;
						if (DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers)) {
							DebugTimeMarkerIndex = 0;
						}
#endif
					}
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