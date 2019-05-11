#include <windows.h>
#include <stdint.h>
#include <xinput.h>

#define internal static 
#define local_persist static
#define global_variable static


struct win32_offscreen_buffer {
	BITMAPINFO info;
	void* memory;
	int width;
	int height;
	int pitch;
	int bytesPerPixel;
};

// TODO: This is a global for now
global_variable bool globalRunning;
global_variable win32_offscreen_buffer globalBackBuffer;


struct win32_window_dimension {
	int width;
	int height;
};

win32_window_dimension
GetWindowDimension(HWND window) {

	win32_window_dimension result;

	RECT clientRect;
	GetClientRect(window, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;

	return(result);
}

internal void
WIN32RenderWeirdGradinent(win32_offscreen_buffer buffer, int xOffset, int yOffset) {

	// TODO: Let's see waht the optimizer does
	uint8_t* row = (uint8_t*)buffer.memory;
	for (int y = 0; y < buffer.height; ++y) {
		uint32_t* pixel = (uint32_t*)row;
		for (int x = 0; x < buffer.width; ++x) {
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

			*pixel++ = (green << 8 | blue);
		}
		row += buffer.pitch;
	}
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer * buffer, int width, int height) {

	if (buffer->memory) {
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}

	buffer->width = width;
	buffer->height = height;
	buffer->bytesPerPixel = 4;

	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = buffer->width;
	buffer->info.bmiHeader.biHeight = -buffer->height;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;

	int bitmapMemorySize = buffer->bytesPerPixel * buffer->width * buffer->height;
	buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	buffer->pitch = buffer->width * buffer->bytesPerPixel;
	// TODO: Probably clear this to black
}

internal void
Win32DisaplayBufferInWindow(HDC deviceContext, int windowWidth, int windowHeight, win32_offscreen_buffer buffer, int x, int y, int width, int height) {
	// TODO: Aspect ration correction
	StretchDIBits(deviceContext,
		0, 0, windowWidth, windowHeight,
		0, 0, buffer.width, buffer.height,
		buffer.memory,
		&buffer.info,
		DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(
	HWND   Window,
	UINT   Message,
	WPARAM wParam,
	LPARAM lParam
) {

	LRESULT result = 0;
	switch (Message) {
	case WM_SIZE: {
		// OutputDebugStringA("WM_SIZE\n");
	}break;
	case WM_DESTROY: {
		// TODO: Handle this with an error
		globalRunning = false;
		OutputDebugStringA("WM_SIZE\n");
	}break;
	case WM_CLOSE: {
		// TODO: Handle with amessage to the user
		globalRunning = false;
		OutputDebugStringA("WM_SIZE\n");
	}break;
	case WM_ACTIVATEAPP: {
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	}break;
	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC deviceContext = BeginPaint(Window, &paint);

		int x = paint.rcPaint.left;
		int y = paint.rcPaint.bottom;
		int height = paint.rcPaint.bottom - paint.rcPaint.top;
		int width = paint.rcPaint.right - paint.rcPaint.left;


		win32_window_dimension dimensions = GetWindowDimension(Window);

		Win32DisaplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, globalBackBuffer, x, y, width, height);

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
WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR     commandLine,
	int       ShowCode
) {

	WNDCLASS windowClass = {};

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
				"Handmade Hero",
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

			int xOffset = 0;
			int yOffset = 0;

			globalRunning = true;
			while (globalRunning) {
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
					}
					else {
						// NOTE: The controller is not available
					}
				}

				WIN32RenderWeirdGradinent(globalBackBuffer, xOffset, yOffset);
				win32_window_dimension dimensions = GetWindowDimension(window);
				Win32DisaplayBufferInWindow(deviceContext, dimensions.width, dimensions.height, globalBackBuffer, 0, 0, dimensions.width, dimensions.height);

				++xOffset;
				++yOffset;
			}

		}
		else {
			// TODO: Logging
		}

	}
	else {
		// TODO: Logging
	}

	return 0;
}