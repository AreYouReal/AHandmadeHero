#pragma once

#include <stdint.h>

struct win32_offscreen_buffer {
	// Pixels are always 32 bits wide. Little endian 0x xx RR GG BB
	BITMAPINFO	info;
	void* memory;
	int			width;
	int			height;
	int			pitch;
	int			BytesPerPixel;
};

struct win32_window_dimension {
	int width;
	int height;
};

struct win32_sound_output {
	int		SamplesPerSecond;
	uint32_t RunningSampleIndex;
	int		BytesPerSample;
	DWORD	SecondaryBufferSize;
	real32	tSine;
	int		LatencySampleCount;
};

struct win32_debug_time_marker{
	DWORD PlayCursor;
	DWORD WriteCursor;
};