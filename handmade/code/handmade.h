#if !defined(HANDMADE_H)

#include "handmade_platform.h"

#define PI 3.1415926535f

/*
	HANDMADE_INTERNAL:s
		0 - Build for public release
		1 - Build for developer only

	HANDMADE_SLOW:
		0 - No slow code allowed!
		1 - Slow code allowed
*/

// MACROS
#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) (Value)*1024 
#define Megabytes(Value) (Kilobytes(Value)*1024)
#define Gigabytes(Value) (Megabytes(Value)*1024) 
#define Terabytes(Value) (Gigabytes(Value)*1024) 

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline uint32 SafeTruncateUInt64(uint64 Value)
{
	Assert(Value <= 0xFFFFFFFF);
	uint32 Result = (uint32)Value;
	return(Result);
}

inline game_controller_input *GetController(game_input *Input, int ControllerIndex)
{
	Assert(ControllerIndex < ArrayCount(Input->Controllers));
	game_controller_input *Result = (&Input->Controllers[ControllerIndex]);
	return Result;
}

#include "handmade_intrinsics.h"
#include "handmade_tile.h"

struct memory_arena
{
	memory_index Size;
	uint8 *Base;
	memory_index Used;
};

struct world
{
	tile_map *TileMap;
};

struct loaded_bitmap
{
	int32 Width;
	int32 Height;
	uint32* Pixels;
};

struct game_state
{
	memory_arena WorldArena;
	world *World;

	tile_map_position PlayerP;
	real32 PlayerSpeed;
	real32 PlayerHeight;
	real32 PlayerWidth;
	
	loaded_bitmap Backdrop;

	loaded_bitmap HeroHead;
	loaded_bitmap HeroCape;
	loaded_bitmap HeroTorso;
};

union RGBReal
{
	real32 d[3];
	struct
	{
		real32 R;
		real32 G;
		real32 B;
	};
};


internal void InitializeArena(memory_arena *Arena, memory_index Size, uint8 *Base)
{
	Arena->Size = Size;
	Arena->Base = Base;
	Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (sizeof(type)*Count))
internal void *PushSize_(memory_arena *Arena, memory_index Size)
{
	Assert((Arena->Used + Size) <= Arena->Size)
	void *Result = Arena->Base + Arena->Used;
	Arena->Used += Size;

	return Result;
}

#define HANDMADE_H
#endif