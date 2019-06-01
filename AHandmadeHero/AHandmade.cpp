#include <stdint.h>

// Handmade headers

#include "AHandmade.h"

static void
RenderWeirdGradinent(game_offscreen_buffer* buffer, int xOffset, int yOffset) {

	// TODO: Let's see want the optimizer does
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

			*pixel++ = (green << 8 | blue);
		}
		row += buffer->pitch;
	}
}

void GameUpdateAndRender(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset){
	RenderWeirdGradinent(Buffer, BlueOffset, GreenOffset);
}
