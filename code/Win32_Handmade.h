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
	DWORD 	SafetyBytes;
	real32	tSine;
	int		LatencySampleCount;
	// TODO: Should running sample index be in bytes as well
	// TODO: Math gets simpler if we add a "bytes per second" field?
};

struct win32_debug_time_marker{
	DWORD OutputPlayCursor;
	DWORD OutputWriteCursor;
	DWORD OutputLocation;
	DWORD OutputByteCount;
	DWORD ExpectedFlipCursor;

	DWORD FlipPlayCursor;
	DWORD FlipWriteCursor;
};