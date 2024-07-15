#include "VirtualMemory.h"

/**
 * \brief Locates a free frame in the RAM and returns its index.
 *		  If no free frames are found, a frame will be evicted,
 *		  and the corresponding index will be returned.
 * \return Index to the newly allocated frame on the RAM.
 */
static int allocate_physical_frame()
{
	return 0;
}

static void traverse_page_table(int frameIndex, int offset)
{
}

void VMinitialize()
{
}

int VMread(uint64_t virtualAddress, word_t* value)
{
	return 0;
}

int VMwrite(uint64_t virtualAddress, word_t value)
{
	return 0;
}