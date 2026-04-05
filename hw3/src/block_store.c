#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "bitmap.h"
#include "block_store.h"
// include more if you need


// Block store struct definition
// Data array holds all blocks contiguously
struct block_store
{
	uint8_t data[BLOCK_STORE_NUM_BYTES];  // 512 blocks * 32 bytes = 16,384 bytes
	bitmap_t *fbm;                         // Free Block Map bitmap overlay
};
// Creates a bitmap to contain n bits (zero initialized)
// \return: A pointer to new block storage and null if failure
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
// Destroys the provided block storage and has no return value
// \param bs - block store device
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
// Searches for a free block, marks it as in use, and returns the block's id
// \param bs - BS device
// \return allocated block's id, SIZE_MAX on error
size_t block_store_allocate(block_store_t *const bs)
{
	if(bs == NULL)
	{
		return SIZE_MAX;
	}
	// Find first 0 in fbm
	size_t block = bitmap_ffz(bs->fbm);

	// No free blocks available
	if(block == SIZE_MAX)
	{
		return SIZE_MAX;
	}
	// Set as used
	bitmap_set(bs->fbm, block);
	return block;
}
// Attempts to allocate the requested block id
// \param bs - the block store object
// \param block_id - the requested block identifier
// \return boolean indicating succes of operation
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
// Frees the specified block
// \param bs - BS device
// \param block_id - The block to free
void block_store_release(block_store_t *const bs, const size_t block_id)
{
	if(bs == NULL)
	{
		return;
	}
	// Prevent out of bounds access
	if(block_id >= BLOCK_STORE_NUM_BLOCKS)
	{
		return;
	}
	// Reset block
	bitmap_reset(bs->fbm, block_id);
}

// Counts the number of blocks marked as in use
// \param bs - the block store object
// \return Total blocks in use, SIZE_MAX on error
size_t block_store_get_used_blocks(const block_store_t *const bs)
{
	if (!bs || !bs->fbm)
    {
        return SIZE_MAX;
    }
	// Counts how many bits are set to 1
    return bitmap_total_set(bs->fbm);
}

// Counts the number of blocks marked free for use
// \param bs BS device
// \return Total blocks free, SIZE_MAX on error
size_t block_store_get_free_blocks(const block_store_t *const bs)
{
	if (!bs || !bs->fbm)
    {
        return SIZE_MAX;
    }
	// Finds how many blocks are in use
    size_t used = block_store_get_used_blocks(bs);
    if (used == SIZE_MAX)
    {
        return SIZE_MAX;
    }
	// Subtracts difference to get free blocks
    return BLOCK_STORE_NUM_BLOCKS - used;
}
// Returns the total number of user-addressable blocks
// \return Total blocks
size_t block_store_get_total_blocks()
{
	return BLOCK_STORE_NUM_BLOCKS;
}

// Reads data from the specified block and writes it to the designated buffer
// \param bs BS device
// \param block_id Source block id
// \param buffer Data buffer to write to
// \return Number of bytes read, 0 on error
size_t block_store_read(const block_store_t *const bs, const size_t block_id, void *buffer)
{
	if (!bs || !buffer || !bs->fbm)
    {
        return 0;
    }

    if (block_id >= BLOCK_STORE_NUM_BLOCKS)
    {
        return 0;
    }
	// Checks if block is in use
    if (!bitmap_test(bs->fbm, block_id))
    {
        return 0;
    }
	// Copies data from block to user buffer
    for (size_t i = 0; i < BLOCK_SIZE_BYTES; i++)
    {
        ((uint8_t *)buffer)[i] = bs->data[block_id * BLOCK_SIZE_BYTES + i];
    }

    return BLOCK_SIZE_BYTES;
}

// Reads data from the specified buffer and writes it to the designated block
// \param bs BS device
// \param block_id Destination block id
// \param buffer Data buffer to read from
// \return Number of bytes written, 0 on error
size_t block_store_write(block_store_t *const bs, const size_t block_id, const void *buffer)
{
	if (!bs || !buffer || !bs->fbm)
    {
        return 0;
    }

    if (block_id >= BLOCK_STORE_NUM_BLOCKS)
    {
        return 0;
    }
	// Checks if block is in use
    if (!bitmap_test(bs->fbm, block_id))
    {
        return 0;
    }
	// Copies data from buffer to block
    for (size_t i = 0; i < BLOCK_SIZE_BYTES; i++)
    {
        bs->data[block_id * BLOCK_SIZE_BYTES + i] = ((const uint8_t *)buffer)[i];
    }

    return BLOCK_SIZE_BYTES;
}

// Imports BS device from the given file
// \param filename The file to load
// \return Pointer to new BS device, NULL on error
block_store_t *block_store_deserialize(const char *const filename)
{
	if(!filename)
	{
		return NULL;
	}
	// Open file
	int fd = open(filename, O_RDONLY);
	if(fd < 0)
	{
		perror("open failed");
		return NULL;
	}
	// Allocate new block
	block_store_t *bs = calloc(1, sizeof(block_store_t));
	if(!bs){
		close(fd);
		return NULL;
	}
	size_t total_read = 0;
	// Loop until expected number of bytes is reached
	while(total_read < BLOCK_STORE_NUM_BYTES)
	{
		ssize_t bytes_read = read(fd, bs->data + total_read, BLOCK_STORE_NUM_BYTES - total_read);
		if(bytes_read < 0)
		{
			perror("read failed");
			close(fd);
			free(bs);
			return NULL;
		}
		// If end of file is reached early
		if(bytes_read == 0)
		{
			break;
		}
		// Amount of successfull bytes read
		total_read += bytes_read;
	}
	close(fd);
	// Pad remaining bytes with 0
	if(total_read < BLOCK_STORE_NUM_BYTES)
	{
		memset(bs->data + total_read, 0, BLOCK_STORE_NUM_BYTES - total_read);
	}
	// Reconnect bitmap to correct location in data
	bs->fbm = bitmap_overlay(BITMAP_SIZE_BITS, bs->data + (BITMAP_START_BLOCK * BLOCK_SIZE_BYTES));
	if(!bs->fbm)
	{
		free(bs);
		return NULL;
	}
	
	return bs;
}

// Writes the entirety of the BS device to file, overwriting it if it exists
// \param bs BS device
// \param filename The file to write to
// \return Number of bytes written, 0 on error
size_t block_store_serialize(const block_store_t *const bs, const char *const filename)
{
	if (!bs || !filename)
    {
        return 0;
    }
	// Open file
	int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0)
	{
		perror("open failed");
		return 0;
	}
	size_t total_written = 0;
	// Loop until all bytes are written
	while(total_written < BLOCK_STORE_NUM_BYTES)
	{
		ssize_t bytes_written = write(fd, bs->data + total_written, BLOCK_STORE_NUM_BYTES - total_written);
		if(bytes_written < 0)
		{
			perror("write failed");
			close(fd);
			return 0 ;
		}
		// successfull amount of writes
		total_written += bytes_written;
	}
	close(fd);
	return total_written;
}
