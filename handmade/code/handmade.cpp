#include "handmade.h"
#include "handmade_random.h"

#include "handmade_tile.cpp"

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer, game_state *GameState)
{	
	int16 ToneVolume = 3000; 	
	//int WavePeriod = SoundBuffer->SamplesPerSecond / GameState->ToneHz;

	int16 *SampleOut = SoundBuffer->Samples;

	for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; SampleIndex++)
	{
		// Region 1			
#if 0
		real32 SineValue = sinf(GameState->tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
#else 
		int16 SampleValue = 0;
#endif
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;

#if 0
		GameState->tSine += 2.0f*PI/(real32)WavePeriod;	
		if(GameState->tSine > (2.0f*PI))
		{
			GameState->tSine -= (2.0f*PI);
		}	
#endif
	}
}

internal void DrawRectangle(game_offscreen_buffer *Buffer, real32 RealMinX, real32 RealMinY, real32 RealMaxX, real32 RealMaxY, RGBReal RGB)
{
	
	int32 MinX = RoundReal32ToInt32(fClamp(RealMinX, 0.0f, (real32)Buffer->Width));
	int32 MaxX = RoundReal32ToInt32(fClamp(RealMaxX, 0.0f, (real32)Buffer->Width));

	int32 MinY = RoundReal32ToInt32(fClamp(RealMinY, 0.0f, (real32)Buffer->Height));	
	int32 MaxY = RoundReal32ToInt32(fClamp(RealMaxY, 0.0f, (real32)Buffer->Height));

	uint32 Color = RGBReal32ToUInt32(RGB.d[0], RGB.d[1], RGB.d[2]);
	
	for(int Y = MinY; Y < MaxY; Y++)
	{		
		uint8 *Pixel = ((uint8 *)Buffer->Memory + MinX*Buffer->BytesPerPixel + Y*Buffer->Pitch);
		for(int X = MinX; X < MaxX; X++)
		{
			*(uint32 *)Pixel = Color;
			Pixel += Buffer->BytesPerPixel;
		}
	}
}

#pragma pack(push, 1)
struct bitmap_header
{
	uint16 FileType;
	uint32 FileSize;
	uint16 Reserved1;
	uint16 Reserved2;
	uint32 BitmapOffset;
	uint32 Size;
	int32 Width;
	int32 Height;
	uint16 Planes;
	uint16 BitsPerPixel;
	uint32 Compression;
	uint32 SizeOfBitmap;
	int32 HorzResolution;
	int32 VertResolution;
	uint32 ColorsUsed;
	uint32 ColorsImportant;
};
#pragma pack(pop)

internal loaded_bitmap DEBUGLoadBMP(thread_context *Thread, debug_platform_read_entire_file *ReadEntireFile, char *Filename)
{

	loaded_bitmap Result = {};

	debug_read_file_result ReadResult = ReadEntireFile(Thread, Filename);	
	if(ReadResult.ContentsSize != 0)
	{
		bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
		uint32 *Pixels = (uint32*)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
		Result.Pixels = Pixels;
		Result.Width = Header->Width;
		Result.Height = Header->Height;

		// NOTE: If you are using this generically, remember that BMP files can go in either direction
		// and the height will be negative for top down
		// Also, there can be compression, etc. Not complete BMP loading code.		
	}

	return Result;
}

internal void DrawBitmap(game_offscreen_buffer *Buffer,loaded_bitmap *Bitmap, real32 RealX, real32 RealY)
{
	int32 MinX = RoundReal32ToInt32(fClamp(RealX, 0.0f, (real32)Buffer->Width));
	int32 MaxX = RoundReal32ToInt32(fClamp(RealX + (real32)Bitmap->Width, 0.0f, (real32)Buffer->Width));

	int32 MinY = RoundReal32ToInt32(fClamp(RealY, 0.0f, (real32)Buffer->Height));	
	int32 MaxY = RoundReal32ToInt32(fClamp(RealY + (real32)Bitmap->Height, 0.0f, (real32)Buffer->Height));

	uint32 *SourceRow = Bitmap->Pixels + (Bitmap->Width*(Bitmap->Height-1));
	uint8 *DestRow = (uint8 *)Buffer->Memory + MinX*Buffer->BytesPerPixel + MinY*Buffer->Pitch;
	for(int32 Y = MinY; Y < MaxY; Y++)
	{
		uint32 *Dest = (uint32 *)DestRow;
		uint32 *Source = (uint32 *)SourceRow;
		for(int32 X = MinX; X < MaxX; X++)
		{
			if(*Source & 0xFF000000){
				*Dest = *Source;
			}
			Dest++;
			Source++;
		}
		DestRow += Buffer->Pitch;
		SourceRow -= Bitmap->Width;
	}
}

// extern "C": Prevents name mangling of compiled function
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{    

	// Assert that the Buttons[] and button struct in the game_controller_input are identical sizes
	// Take the last know button, subtract the base address, and the value should be equal the the number of entries
	Assert((&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0]) == 
			(ArrayCount(Input->Controllers[0].Buttons) - 1));

	// Assert that our game state will fit in the allocated permanent memory
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);	

	game_state *GameState = (game_state*)Memory->PermanentStorage;
	if(!Memory->IsInitialized)
	{		
		GameState->Backdrop = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/test_background.bmp");

		GameState->HeroHead = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/test_hero_front_head.bmp");
		GameState->HeroCape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/test_hero_front_cape.bmp");
		GameState->HeroTorso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/test_hero_front_torso.bmp");

		GameState->PlayerP.AbsTileX = 1;
		GameState->PlayerP.AbsTileY = 3;
		GameState->PlayerP.OffsetX = 5.0f;
		GameState->PlayerP.OffsetY = 5.0f;
		// Meters per Second
		GameState->PlayerHeight = 1.4f;
		GameState->PlayerWidth = GameState->PlayerHeight*0.75f;
		GameState->PlayerSpeed = 2.15f;

		InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state), 
						(uint8 *)Memory->PermanentStorage + sizeof(game_state));

		GameState->World = PushStruct(&GameState->WorldArena, world);
		world *World = GameState->World;
		World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

		tile_map *TileMap = World->TileMap;

		// Set to using 256x256 Tile Chunks
		TileMap->ChunkShift = 4;
		TileMap->ChunkMask = (0x1 << TileMap->ChunkShift) - 1; // bit magic B)
		TileMap->ChunkDim = 0x1 << TileMap->ChunkShift;

		TileMap->TileChunkCountX = 128;
		TileMap->TileChunkCountY = 128;
		TileMap->TileChunkCountZ = 2;

		TileMap->TileChunks = PushArray(&GameState->WorldArena, 
										TileMap->TileChunkCountX*TileMap->TileChunkCountY*TileMap->TileChunkCountZ, 
										tile_chunk);

		TileMap->TileSideInMeters = 1.4f;					

		uint32 TilesPerWidth = 17;
		uint32 TilesPerHeight = 9;

		uint32 ScreenX = 0;
		uint32 ScreenY = 0;
		uint32 RandomNumberIndex = 0;
		bool32 DoorLeft = false;
		bool32 DoorRight = false;
		bool32 DoorTop = false;
		bool32 DoorBottom = false;
		bool32 DoorUp = false;
		bool32 DoorDown = false;
		uint32 AbsTileZ = 0;
		for(uint32 ScreenIndex = 0; ScreenIndex < 100; ScreenIndex++)
		{
			// TODO Random Number Generator
			Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
			uint32 RandomChoice;
			if(DoorUp || DoorDown)
			{
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
			}
			else{
				RandomChoice = RandomNumberTable[RandomNumberIndex++] % 3;
			}

			bool32 CreatedZDoor = false;
			if(RandomChoice == 2)
			{
				CreatedZDoor = true;
				if(AbsTileZ == 0)
				{
					DoorUp = true;
				}
				else{
					DoorDown = true;
				}
			}
			else if(RandomChoice == 1)
			{
				DoorRight = true;
			}
			else
			{
				DoorTop = true;
			}
			
			for(uint32 TileY = 0; TileY < TilesPerHeight; TileY++)
			{
				for(uint32 TileX = 0; TileX < TilesPerWidth; TileX++)
				{
					uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
					uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;					

					uint32 TileValue = 1;
					if((TileX == 0) && (!DoorLeft || (TileY != (TilesPerHeight/2))))
					{
						TileValue = 2;					
					}
					if((TileX == (TilesPerWidth - 1)) && (!DoorRight || (TileY != (TilesPerHeight/2))))
					{
						TileValue = 2;					
					}								
					if((TileY == 0) && (!DoorBottom ||TileX != (TilesPerWidth/2)))
					{						
						TileValue = 2;							
					}					
					if((TileY == (TilesPerHeight - 1)) && (!DoorTop ||TileX != (TilesPerWidth/2)))
					{						
						TileValue = 2;							
					}

					if((TileX == 10) && (TileY == 6))
					{
						if(DoorUp)
						{
							TileValue = 3;
						}
						
						if(DoorDown)
						{
							TileValue = 4;
						}
						
					}

					SetTileValue(&GameState->WorldArena, World->TileMap, 
								AbsTileX, AbsTileY, AbsTileZ,
								TileValue);
				}
			}
			
			DoorLeft = DoorRight;
			DoorBottom = DoorTop;

			if(CreatedZDoor)
			{			
				DoorDown = !DoorDown;
				DoorUp = !DoorUp;
			}
			else
			{
				DoorUp = false;
				DoorDown = false;
			}

			DoorRight = false;
			DoorTop = false;
		
			if(RandomChoice == 2)
			{
				if(AbsTileZ == 0)
				{
					AbsTileZ = 1;
				}
				else
				{
					AbsTileZ = 0;
				}
			}
			else if(RandomChoice == 1)
			{
				ScreenX += 1;
			}
			else{
				ScreenY += 1;
			}
		}


		
		Memory->IsInitialized = true;
	}						

	world *World = GameState->World;
	tile_map *TileMap = World->TileMap;

	int32 TileSideInPixels = 60;
	real32 MetersToPixels = (real32)(TileSideInPixels / TileMap->TileSideInMeters);

	for(int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex)
	{	
		game_controller_input *Controller = GetController(Input, ControllerIndex);

		if(Controller->IsAnalog)
		{	

		}
		else
		{
			real32 dPlayerX = 0.0f;
			real32 dPlayerY = 0.0f;

			if(Controller->MoveUp.EndedDown)
			{
				dPlayerY = 1.0f;
			}
			if(Controller->MoveDown.EndedDown)
			{
				dPlayerY = -1.0f;
			}
			if(Controller->MoveLeft.EndedDown)
			{
				dPlayerX = -1.0f;
			}
			if(Controller->MoveRight.EndedDown)
			{
				dPlayerX = 1.0f;
			}

			dPlayerX *= GameState->PlayerSpeed;
			dPlayerY *= GameState->PlayerSpeed;
			if(Controller->ActionUp.EndedDown)
			{
				dPlayerX *= 8;
				dPlayerY *= 8;
			}	

			tile_map_position NewPlayerP = GameState->PlayerP;
			NewPlayerP.OffsetX += (dPlayerX * Input->dtForFrame);
			NewPlayerP.OffsetY += (dPlayerY * Input->dtForFrame);			
			NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);

			tile_map_position PlayerLeft = NewPlayerP;
			PlayerLeft.OffsetX -= 0.5f*GameState->PlayerWidth;
			PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

			tile_map_position PlayerRight = NewPlayerP;
			PlayerRight.OffsetX += 0.5f*GameState->PlayerWidth;
			PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);
			
			if(IsTileMapPointEmpty(TileMap, NewPlayerP) &&
				IsTileMapPointEmpty(TileMap, PlayerLeft) &&
				IsTileMapPointEmpty(TileMap, PlayerRight))
			{
				if(!AreOnSameTile(&GameState->PlayerP, &NewPlayerP))
				{
					uint32 NewTileValue = GetTileValue(TileMap, NewPlayerP);

					if(NewTileValue == 3)
					{
						++NewPlayerP.AbsTileZ;
					}
					else if(NewTileValue == 4)
					{
						--NewPlayerP.AbsTileZ;
					}
				}
				GameState->PlayerP = NewPlayerP;
			}
		}
	}	


	DrawBitmap(Buffer, &GameState->Backdrop, 0.0f, 0.0f);

	real32 ScreenCenterX = 0.5f*(real32)Buffer->Width;
	real32 ScreenCenterY = 0.5f*(real32)Buffer->Height;

	for(int32 RelRow = -10; RelRow < 10; RelRow++)
	{
		for(int32 RelCol = -20; RelCol < 20; RelCol++)
		{

			uint32 Col = RelCol + GameState->PlayerP.AbsTileX;
			uint32 Row = RelRow  + GameState->PlayerP.AbsTileY;
			uint32 TileID = GetTileValue(TileMap, Col, Row, GameState->PlayerP.AbsTileZ);
			if(TileID > 1){		
				real32 Gray = 0.5f;				
				if(TileID == 2)
				{
					Gray = 1.0f;
				}	
				if(TileID > 2)
				{
					Gray = 0.2f;
				}	

				if((Row == GameState->PlayerP.AbsTileY) && (Col == GameState->PlayerP.AbsTileX))
				{
					Gray = 0.0f;
				}

				
				real32 CenX = ScreenCenterX - MetersToPixels*GameState->PlayerP.OffsetX + (real32)RelCol*TileSideInPixels;
				real32 CenY = ScreenCenterY + MetersToPixels*GameState->PlayerP.OffsetY - (real32)RelRow*TileSideInPixels;
				real32 MinX = CenX - 0.5f*TileSideInPixels;
				real32 MinY = CenY - 0.5f*TileSideInPixels;
				real32 MaxX = CenX + 0.5f*TileSideInPixels;
				real32 MaxY = CenY + 0.5f*TileSideInPixels;
				RGBReal TileColor = {Gray, Gray, Gray};
				DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, TileColor);
			}
		}
	}	

	RGBReal PlayerColor = {0.8f, 0.8f, 0.0f};
	real32 PlayerLeft =	ScreenCenterX - MetersToPixels*0.5f*GameState->PlayerWidth;
	real32 PlayerTop = ScreenCenterY - MetersToPixels*GameState->PlayerHeight;	
	DrawRectangle(Buffer, 
				  PlayerLeft, PlayerTop, 
				  PlayerLeft + MetersToPixels*GameState->PlayerWidth, PlayerTop + MetersToPixels*GameState->PlayerHeight,
				  PlayerColor);
	DrawBitmap(Buffer, &GameState->HeroCape, PlayerLeft, PlayerTop);
	DrawBitmap(Buffer, &GameState->HeroTorso, PlayerLeft, PlayerTop);
	DrawBitmap(Buffer, &GameState->HeroHead, PlayerLeft, PlayerTop);
}


extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	// TODO : may not want continuous samples, could require earlier or later samples	
	game_state *GameState = (game_state*)Memory->PermanentStorage;
	GameOutputSound(SoundBuffer, GameState);
}

/*
internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	for (int Y = 0; Y < Buffer->Height; Y++)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer->Width; X++)
		{
			// Pixel in memory: BB GG RR XX
			// 0x0RGB
			uint8 Blue = (uint8)(X + XOffset);
			uint8 Green = (uint8)(Y + YOffset);

			*Pixel++ = (Green << 16) | Blue;
		}
		Row += Buffer->Pitch;
	}


}
*/