#ifndef HANDMADE_TILE_H

struct tile_map_difference
{
	real32 dX;
	real32 dY;
	real32 dZ;
};

typedef struct 
{
	// Fixed bit tile locations. high bits are the tile chunk index, low bits are tile index
	// in chunk.
	uint32 AbsTileX;
	uint32 AbsTileY;
	uint32 AbsTileZ;

	// NOTE: These are the offsets from center of tile
	real32 OffsetX;
	real32 OffsetY;
} tile_map_position;

typedef struct 
{
	uint32 TileChunkX;
	uint32 TileChunkY;
	uint32 TileChunkZ;	

	uint32 RelTileX;
	uint32 RelTileY;
} tile_chunk_position;

typedef struct 
{
	uint32 *Tiles;
} tile_chunk;

typedef struct  
{
    uint32 ChunkShift;
	uint32 ChunkMask;
	uint32 ChunkDim;

	real32 TileSideInMeters;

	// TODO REAL sparseness?
	uint32 TileChunkCountX;
	uint32 TileChunkCountY;	
	uint32 TileChunkCountZ;	
	tile_chunk *TileChunks;
} tile_map;


#define HANDMADE_TILE_H
#endif