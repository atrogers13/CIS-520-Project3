#include <stdio.h>
#include <stdint.h>

#include "bitmap.h"
#include "block_store.h"
// include more if you need


// You might find this handy. I put it around unused parameters, but you should
// remove it before you submit. Just allows things to compile initially.
#define UNUSED(x) (void)(x)

// Block store struct definition
// Data array holds all blocks contiguously
struct block_store
{
	uint8_t data[BLOCK_STORE_NUM_BYTES];  // 512 blocks * 32 bytes = 16,384 bytes
	bitmap_t *fbm;                         // Free Block Map bitmap overlay
};

block_store_t *block_store_create()
{
	// Allocate and zero-initialize the block store
	block_store_t *bs = calloc(1, sizeof(block_store_t));
	if (!bs)
	{
		return NULL;
	}

	// Create a bitmap overlay at BITMAP_START_BLOCK within the data array
	// The bitmap tracks all BITMAP_SIZE_BITS (512) blocks
	bs->fbm = bitmap_overlay(BITMAP_SIZE_BITS, &bs->data[BITMAP_START_BLOCK * BLOCK_SIZE_BYTES]);
	if (!bs->fbm)
	{
		free(bs);
		return NULL;
	}

	// Mark the blocks used by the bitmap as allocated
	for (size_t i = BITMAP_START_BLOCK; i < BITMAP_START_BLOCK + BITMAP_NUM_BLOCKS; i++)
	{
		block_store_request(bs, i);
	}

	return bs;
}

void block_store_destroy(block_store_t *const bs)
{
	if (bs)
	{
		// Destroy the bitmap first (overlay doesn't free underlying data)
		bitmap_destroy(bs->fbm);
		// Free the block store struct
		free(bs);
	}
}

size_t block_store_allocate(block_store_t *const bs)
{
	UNUSED(bs);
	return 0;
}

bool block_store_request(block_store_t *const bs, const size_t block_id)
{
	// Validate parameters
	if (!bs || block_id >= BLOCK_STORE_NUM_BLOCKS)
	{
		return false;
	}

	// Check if the block is already allocated
	if (bitmap_test(bs->fbm, block_id))
	{
		return false;
	}

	// Mark the block as allocated
	bitmap_set(bs->fbm, block_id);

	// Verify the block was successfully marked
	return bitmap_test(bs->fbm, block_id);
}

void block_store_release(block_store_t *const bs, const size_t block_id)
{
	UNUSED(bs);
	UNUSED(block_id);
}

size_t block_store_get_used_blocks(const block_store_t *const bs)
{
	UNUSED(bs);
	return 0;
}

size_t block_store_get_free_blocks(const block_store_t *const bs)
{
	UNUSED(bs);
	return 0;
}

size_t block_store_get_total_blocks()
{
	return 0;
}

size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer)
{
	UNUSED(bs);
	UNUSED(block_id);
	UNUSED(buffer);
	return 0;
}

size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer)
{
	UNUSED(bs);
	UNUSED(block_id);
	UNUSED(buffer);
	return 0;
}

block_store_t *block_store_deserialize(const char *const filename)
{
	UNUSED(filename);
	return NULL;
}

size_t block_store_serialize(const block_store_t *const bs, const char *const filename)
{
	UNUSED(bs);
	UNUSED(filename);
	return 0;
}
