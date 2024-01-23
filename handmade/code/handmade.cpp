#include "handmade.h"

#include "handmade_tile.cpp"
#include <stdio.h>

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

internal void DrawRectangle(game_offscreen_buffer *Buffer, v2 vMin, v2 vMax, RGBReal RGB)
{
	
	int32 MinX = RoundReal32ToInt32(fClamp(vMin.X, 0.0f, (real32)Buffer->Width));
	int32 MaxX = RoundReal32ToInt32(fClamp(vMax.X, 0.0f, (real32)Buffer->Width));

	int32 MinY = RoundReal32ToInt32(fClamp(vMin.Y, 0.0f, (real32)Buffer->Height));	
	int32 MaxY = RoundReal32ToInt32(fClamp(vMax.Y, 0.0f, (real32)Buffer->Height));

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

	uint32 RedMask;
	uint32 GreenMask;
	uint32 BlueMask;
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

		// Early break if compression value is not 3
		if(Header->Compression != 3) return Result;


		// NOTE: Byte order in memory is determined by the Header itself when compression = 3,
		// we have to read out the masks and convert the pixels ourselves
		uint32 RedMask = Header->RedMask;
		uint32 GreenMask = Header->GreenMask;
		uint32 BlueMask = Header->BlueMask;
		uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

		bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
		bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
		bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
		bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);		

		Assert(RedScan.Found);
		Assert(GreenScan.Found);
		Assert(BlueScan.Found);
		Assert(AlphaScan.Found);
		
		int32 RedShift = 16 - (int32)RedScan.Index;
		int32 GreenShift = 8 - (int32)GreenScan.Index;
		int32 BlueShift = 0 - (int32)BlueScan.Index;
		int32 AlphaShift = 24 - (int32)AlphaScan.Index;

		uint32 *SourceDest = Pixels;
		for(int32 Y = 0; Y < Header->Height; Y++)
		{
			for(int32 X = 0; X < Header->Width; X++)
			{
				uint32 C = *SourceDest;

				uint32 Result = _rotl(RedMask, RedShift);

				*SourceDest++ = (RotateLeft(C & RedMask, RedShift) |
								RotateLeft(C & GreenMask, GreenShift) | 
								RotateLeft(C & BlueMask, BlueShift) | 
								RotateLeft(C & AlphaMask, AlphaShift));	
			}
		}
	}	


	return Result;
}

internal void DrawBitmap(game_offscreen_buffer *Buffer, loaded_bitmap *Bitmap, real32 RealX, real32 RealY, int32 AlignX = 0, int32 AlignY = 0)
{	

	RealX -= (real32)AlignX;
	RealY -= (real32)AlignY;	

	int32 MinX = TruncateReal32ToInt32(RealX);
	int32 MinY = TruncateReal32ToInt32(RealY);
	int32 MaxX = TruncateReal32ToInt32(RealX + (real32)Bitmap->Width);
	int32 MaxY = TruncateReal32ToInt32(RealY + (real32)Bitmap->Height);

	int32 SourceOffsetX = 0;
	int32 SourceOffsetY = 0;
	if(MinX < 0)
	{
		SourceOffsetX = -MinX;
		MinX = 0;
	}
	if(MinY < 0)
	{
		SourceOffsetY = -MinY;
		MinY = 0;
	}
	if(MaxX > Buffer->Width)
	{	
		MaxX = Buffer->Width;
	}
	if(MaxY > Buffer->Height)
	{
		MaxY = Buffer->Height;
	}
	
	// TODO WE CRASHING HERE :( but I don't want to fix right now
	uint32 *SourceRow = Bitmap->Pixels + (Bitmap->Width*(Bitmap->Height-1));
	SourceRow += -SourceOffsetY*Bitmap->Width + SourceOffsetX;
	uint8 *DestRow = (uint8 *)Buffer->Memory + MinX*Buffer->BytesPerPixel + MinY*Buffer->Pitch;
	for(int32 Y = MinY; Y < MaxY; Y++)
	{
		uint32 *Dest = (uint32 *)DestRow;
		uint32 *Source = (uint32 *)SourceRow;
		for(int32 X = MinX; X < MaxX; X++)
		{
			real32 A = ((real32)((*Source >> 24) & 0xFF) / 255.0f);
			real32 SR = (real32)((*Source >> 16) & 0xFF);
			real32 SG = (real32)((*Source >> 8) & 0xFF);
			real32 SB = (real32)((*Source >> 0) & 0xFF);

			real32 DR = (real32)((*Dest >> 16) & 0xFF);
			real32 DG = (real32)((*Dest >> 8) & 0xFF);
			real32 DB = (real32)((*Dest >> 0 )& 0xFF);

			real32 R = (1.0f - A)*DR + A*SR;
			real32 G = (1.0f - A)*DG + A*SG;
			real32 B = (1.0f - A)*DB + A*SB;

			*Dest =	((*Source & 0xFF000000) | 
					((uint32)(R + 0.5f) << 16) |
					((uint32)(G + 0.5f) << 8) |
					(uint32)(B + 0.5f)); 
						

			Dest++;
			Source++;
		}
		DestRow += Buffer->Pitch;
		SourceRow -= Bitmap->Width;
	}
}

internal entity* GetEntity(game_state *GameState, uint32 Index)
{
	entity *Entity = 0;
	if((Index > 0) & (Index < ArrayCount(GameState->Entities)))
	{
		Entity = &GameState->Entities[Index];
	}

	return Entity;
}

internal uint32 AddEntity(game_state *GameState)
{
	uint32 EntityIndex = GameState->EntityCount++;
	Assert(GameState->EntityCount < ArrayCount(GameState->Entities));
	entity *Entity = &GameState->Entities[EntityIndex];
	*Entity = {};

	return EntityIndex;
}

internal void InitializePlayer(game_state *GameState, uint32 EntityIndex)
{	
	entity *Entity = GetEntity(GameState, EntityIndex);
	Entity->Exists = true;
	Entity->P.AbsTileX = 1;
	Entity->P.AbsTileY = 3;
	Entity->P.Offset.X = 5.0f;
	Entity->P.Offset.Y = 5.0f;
	Entity->Height = 1.4f;
	Entity->Width = Entity->Height*0.75f;

	if(!GetEntity(GameState, GameState->CameraFollowingEntityIndex))
	{
		GameState->CameraFollowingEntityIndex = EntityIndex;
	}
}

internal void TestWall(real32 Wall, real32 RelX, real32 RelY, real32 PlayerDeltaX, 
						real32 PlayerDeltaY, real32 *tMin, real32 MinY, real32 MaxY)
{	
	real32 tEpsilon = 0.001f;
	if (PlayerDeltaX != 0.0f)
	{		
		real32 tResult = (Wall - RelX) / PlayerDeltaX;
		real32 Y = RelY + tResult * PlayerDeltaY;
		if ((tResult > 0.0f) && (*tMin > tResult))
		{
			// Wall exists at this Y
			if ((Y >= MinY) && (Y <= MaxY))
			{
				*tMin = Maximum(0.0f, tResult-tEpsilon);
			}
		}
	}
}

internal void MovePlayer(game_state *GameState, entity *Entity, real32 dt, v2 ddP)
{	
	tile_map *TileMap = GameState->World->TileMap;

	real32 ddPLength2 = LengthSq(ddP);
	// Max acceleration = 1, check against 1^2 (= 1)
	if(ddPLength2 > 1.0f)
	{
		ddP *= (1.0f / SquareRoot(ddPLength2));
	}	

	real32 Speed = 8.0f;
	ddP *= Speed;	

	// TODO ODE here! (Friction)
	ddP += -1.2f * Entity->dP;

	tile_map_position OldPlayerP = Entity->P;	
	// Update position and velocity
	v2 PlayerDelta = (0.5f * (ddP*Square(dt)) + (Entity->dP * dt));
	Entity->dP = ddP*dt + Entity->dP;				
	tile_map_position NewPlayerP = OldPlayerP;
	NewPlayerP.Offset += PlayerDelta; 											
	NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);

#if 0			

	tile_map_position PlayerLeft = NewPlayerP;
	PlayerLeft.Offset.X -= 0.5f*Entity->Width;
	PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);

	tile_map_position PlayerRight = NewPlayerP;
	PlayerRight.Offset.X += 0.5f*Entity->Width;
	PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);
	

	bool32 Collided = false;
	tile_map_position ColP = {};
	if(!IsTileMapPointEmpty(TileMap, NewPlayerP))
	{
		Collided = true;
		ColP = NewPlayerP;
	}
	if(!IsTileMapPointEmpty(TileMap, PlayerLeft))
	{
		Collided = true;
		ColP = PlayerLeft;
	}
	if(!IsTileMapPointEmpty(TileMap, PlayerRight))
	{
		Collided = true;
		ColP = PlayerRight;
	}
	if(Collided)
	{
		v2 R = {};
		if(ColP.AbsTileX < Entity->P.AbsTileX)
		{
			// Left wall;
			R = V2(1, 0);
		}
		if(ColP.AbsTileX > Entity->P.AbsTileX)
		{
			// Right wall;
			R = V2(-1, 0);
		}
		if(ColP.AbsTileY < Entity->P.AbsTileY)
		{
			R = V2(0, 1);
		}
		if(ColP.AbsTileY > Entity->P.AbsTileY)
		{
			R = V2(0, -1);
		}
		Entity->dP = Entity->dP - 1 * Inner(Entity->dP, R) * R;
	}
	else
	{
		Entity->P = NewPlayerP;
	}
#else	
	uint32 MinTileX = Minimum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX);
	uint32 MinTileY = Minimum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY);
	uint32 OnePastMaxTileX = Maximum(OldPlayerP.AbsTileX, NewPlayerP.AbsTileX) + 1;
	uint32 OnePastMaxTileY = Maximum(OldPlayerP.AbsTileY, NewPlayerP.AbsTileY) + 1;

	uint32 AbsTileZ = Entity->P.AbsTileZ;	
	real32 tMin = 1.0f;
	for(uint32 AbsTileY = MinTileY; AbsTileY != OnePastMaxTileY; AbsTileY++)
	{
		for(uint32 AbsTileX = MinTileX; AbsTileX != OnePastMaxTileX; AbsTileX++)
		{
			tile_map_position TestTileP = CenteredTilePoint(AbsTileX, AbsTileY, AbsTileZ);
			uint32 TileValue = GetTileValue(TileMap, TestTileP);
			if(!IsTileValueEmpty(TileValue))
			{
				v2 MinCorner = -0.5f*v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};
				v2 MaxCorner = 0.5f*v2{TileMap->TileSideInMeters, TileMap->TileSideInMeters};				

				// Vector from center of tile to player position
				tile_map_difference RelOldPlayerP = Subtract(TileMap, &OldPlayerP, &TestTileP);
				v2 Rel = RelOldPlayerP.dXY;			

				// Test all four walls and take the minimum t				
				TestWall(MaxCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y);
				TestWall(MinCorner.X, Rel.X, Rel.Y, PlayerDelta.X, PlayerDelta.Y, &tMin, MinCorner.Y, MaxCorner.Y);
				TestWall(MaxCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X);
				TestWall(MinCorner.Y, Rel.Y, Rel.X, PlayerDelta.Y, PlayerDelta.X, &tMin, MinCorner.X, MaxCorner.X);									
			}
		}
	}	
	// TODO: these wall tests are working ?? but don't stop the player movement through the wall				
	NewPlayerP = OldPlayerP;
	NewPlayerP.Offset += tMin*PlayerDelta;
	Entity->P = NewPlayerP;
	NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);

#endif

	// NOTE: update camera/player Z based on last movement
	if(!AreOnSameTile(&OldPlayerP, &Entity->P))
	{
		uint32 NewTileValue = GetTileValue(TileMap, Entity->P);

		if(NewTileValue == 3)
		{
			++Entity->P.AbsTileZ;
		}
		else if(NewTileValue == 4)
		{
			--Entity->P.AbsTileZ;
		}
	}

	if((Entity->dP.X == 0.0f) && (Entity->dP.Y == 0.0f))
	{
		// NOTE: Leave FacingDirection how it was
	}
	else if(AbsoluteValue(Entity->dP.X) > AbsoluteValue(Entity->dP.Y))
	{
		if(Entity->dP.X > 0)
		{
			Entity->FacingDirection = 0;
		}
		else
		{
			Entity->FacingDirection = 2;
		}
	}
	else
	{
		if(Entity->dP.Y > 0)
		{
			Entity->FacingDirection = 1;
		}
		else
		{
			Entity->FacingDirection = 3;
		}
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
		//NOTE:  Reserve slot 0	for null entity
		AddEntity(GameState);

		GameState->Backdrop = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");

		hero_bitmaps *Bitmap;

		Bitmap = &GameState->HeroBitmaps[0];
		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_right_torso.bmp");
		Bitmap->AlignX = 76;
		Bitmap->AlignY = 182;		

		Bitmap++;
		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_back_torso.bmp");
		Bitmap->AlignX = 71;
		Bitmap->AlignY = 181;				

		Bitmap++;
		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_left_torso.bmp");
		Bitmap->AlignX = 66;
		Bitmap->AlignY = 181;					

		Bitmap++;
		Bitmap->Head = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
		Bitmap->Cape = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
		Bitmap->Torso = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp");
		Bitmap->AlignX = 71;
		Bitmap->AlignY = 181;					

		GameState->CameraP.AbsTileX = 17/2;
		GameState->CameraP.AbsTileY = 9/2;				

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
		entity *ControllingEntity = GetEntity(GameState, GameState->PlayerIndexForController[ControllerIndex]);
		if(ControllingEntity)	
		{
			v2 ddP = {};
			bool32 didInputMovement = false;
			if(Controller->IsAnalog)
			{	
				// NOTE: analog movement tuning
				ddP = V2(Controller->StickAverageX, Controller->StickAverageY);
			}
			else
			{								
				
				if(Controller->MoveUp.EndedDown)
				{
					ddP.Y = 1.0f;		
					didInputMovement = true;		
				}
				if(Controller->MoveDown.EndedDown)
				{
					ddP.Y = -1.0f;
					didInputMovement = true;
				}
				if(Controller->MoveLeft.EndedDown)
				{
					ddP.X = -1.0f;
					didInputMovement = true;
				}
				if(Controller->MoveRight.EndedDown)
				{
					ddP.X = 1.0f;
					didInputMovement = true;
				}							
			}

			if(didInputMovement){
				MovePlayer(GameState, ControllingEntity, Input->dtForFrame, ddP);
			}			
			else
			{
				MovePlayer(GameState, ControllingEntity, Input->dtForFrame, ddP);
			}
		}		
		else
		{
			if(Controller->Start.EndedDown)
			{
				uint32 EntityIndex = AddEntity(GameState);				
				InitializePlayer(GameState, EntityIndex);
				GameState->PlayerIndexForController[ControllerIndex] = EntityIndex;
			}		
		}
	}	

	entity *CameraFollowingEntity = GetEntity(GameState, GameState->CameraFollowingEntityIndex);
	if(CameraFollowingEntity)
	{		
		GameState->CameraP.AbsTileZ = CameraFollowingEntity->P.AbsTileZ;	

		tile_map_difference Diff = Subtract(TileMap, &CameraFollowingEntity->P, &GameState->CameraP);
		if(Diff.dXY.X > (9.0f*TileMap->TileSideInMeters))
		{
			GameState->CameraP.AbsTileX += 17;
		}
		if(Diff.dXY.X < -(9.0f*TileMap->TileSideInMeters))
		{
			GameState->CameraP.AbsTileX -= 17;
		}
		if(Diff.dXY.Y > (5.0f*TileMap->TileSideInMeters))
		{
			GameState->CameraP.AbsTileY += 9;
		}
		if(Diff.dXY.Y < -(5.0f*TileMap->TileSideInMeters))
		{
			GameState->CameraP.AbsTileY -= 9;
		}		
	}	

	// NOTE: Render
	DrawBitmap(Buffer, &GameState->Backdrop, 0.0f, 0.0f);

	real32 ScreenCenterX = 0.5f*(real32)Buffer->Width;
	real32 ScreenCenterY = 0.5f*(real32)Buffer->Height;

	for(int32 RelRow = -10; RelRow < 10; RelRow++)
	{
		for(int32 RelCol = -20; RelCol < 20; RelCol++)
		{

			uint32 Col = RelCol + GameState->CameraP.AbsTileX;
			uint32 Row = RelRow  + GameState->CameraP.AbsTileY;
			uint32 TileID = GetTileValue(TileMap, Col, Row, GameState->CameraP.AbsTileZ);
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

				if((Row == GameState->CameraP.AbsTileY) && (Col == GameState->CameraP.AbsTileX))
				{
					Gray = 0.0f;
				}

				

				v2 HalfTileSide = {0.5f*TileSideInPixels, 0.5f*TileSideInPixels};
				v2 Cen = {ScreenCenterX - MetersToPixels*GameState->CameraP.Offset.X + (real32)RelCol*TileSideInPixels,
							ScreenCenterY + MetersToPixels*GameState->CameraP.Offset.Y - (real32)RelRow*TileSideInPixels};
				v2 Min = Cen - HalfTileSide;				
				v2 Max = Cen + HalfTileSide;				
				RGBReal TileColor = {Gray, Gray, Gray};
				DrawRectangle(Buffer, Min, Max, TileColor);
			}
		}
	}		

	entity *Entity = GameState->Entities;
	for(uint32 EntityIndex = 0; EntityIndex < GameState->EntityCount; EntityIndex++, ++Entity)
	{		
		// TODO Culling of entities based on Z camera view
		if(Entity->Exists)
		{
			tile_map_difference Diff = Subtract(TileMap, &Entity->P, &GameState->CameraP);

			RGBReal EntityColor = {0.8f, 0.8f, 0.0f};
			real32 EntityGroundX = ScreenCenterX + MetersToPixels*Diff.dXY.X;
			real32 EntityGroundY = ScreenCenterY - MetersToPixels*Diff.dXY.Y;
			v2 EntityLeftTop =	{EntityGroundX - MetersToPixels*0.5f*Entity->Width,
								EntityGroundY - MetersToPixels*Entity->Height};
			v2 EntityWidthHeight = {Entity->Width, Entity->Height};
			DrawRectangle(Buffer, 
						EntityLeftTop,
						EntityLeftTop + MetersToPixels*EntityWidthHeight,
						EntityColor);
					
			hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[Entity->FacingDirection];			

			DrawBitmap(Buffer, &HeroBitmaps->Torso, EntityGroundX, EntityGroundY, 
						HeroBitmaps->AlignX, HeroBitmaps->AlignY);
			DrawBitmap(Buffer, &HeroBitmaps->Cape, EntityGroundX, EntityGroundY, 
						HeroBitmaps->AlignX, HeroBitmaps->AlignY);	
			DrawBitmap(Buffer, &HeroBitmaps->Head, EntityGroundX, EntityGroundY, 
						HeroBitmaps->AlignX, HeroBitmaps->AlignY);		
		}
	}

	  
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