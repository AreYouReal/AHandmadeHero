#pragma once


/*
	TODO: Services that the platform layer provides to the game
*/

/* 
	NOTE: Services that the game provides to the platform layer
	(this may expand in the future - sound on separate thread, etc.)
*/


struct game_offscreen_buffer {
	// Pixels are always 32 bits wide. Little endian 0x xx RR GG BB
	void* memory;
	int			width;
	int			height;
	int			pitch;
};


// Three things - timing, Controller/keyboard input, bitmap buffer to use, sound buffer to use
/*static*/ void GameUpdateAndRender(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset);
