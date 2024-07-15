#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include "Utils.h"

/**
 * NOTE: It's a global convention in this project to use 'pa' as an
 *		 abbreviation for 'physical address' and 'va' as an abbreviation for
 *		 'virtual address'.
 */

constexpr uint64_t ROOT_FRAME = 0;

static word_t pa_frame_read_word(uint64_t frame, uint64_t offset)
{
	word_t word = 0;
	PMread((frame * PAGE_SIZE) + offset, &word);
	return word;
}

static void pa_frame_write_word(uint64_t frame, uint64_t offset, word_t value)
{
	PMwrite((frame * PAGE_SIZE) + offset, value);
}

static void zero_frame(uint64_t frame)
{
	for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset)
	{
		pa_frame_write_word(frame, offset, 0);
	}
}

/**
 * \brief Locates a free frame in the RAM and returns its index.
 *		  If no free frames are found, a frame will be evicted,
 *		  and the corresponding index will be returned.
 * \return Index to the newly allocated frame on the RAM.
 */
static int allocate_physical_frame()
{
	uint64_t next_node = pa_frame_read_word(
		ROOT_FRAME, Utils::va_get_page_index_depth(va, frame));
	if (ROOT_FRAME == next_node) // The page is not mapped
	{

	}
	else // The page is mapped, go further one step down the tree
	{
		traverse_page_table(frame + 1);
	}
	return 0;
}

/**
 * \brief Traversing the page table from the given frame node.
 *		  This is essentially an implementation of DFS.
 * \param va 
 * \param depth 
 */
static word_t traverse_page_table(
	word_t frame, 
	uint64_t depth, 
	bool* empty_table,
	uint64_t target_page,
	uint64_t* max_dist, 
	word_t* max_dist_frame,
	word_t* max_dist_page)
{
	// Stopping criteria - we reached the leaves of the page table tree
	if (TABLES_DEPTH == depth)
	{
		// Iterating on all leaves of the page table
		for (uint64_t word_offset = 0; word_offset < PAGE_SIZE; ++word_offset)
		{
			const word_t mapped_page = pa_frame_read_word(frame, word_offset);
			// If the target frame is mapped to RAM, we measure the
			// cyclical distance from the page in va to it
			if (ROOT_FRAME != mapped_page)
			{
				const uint64_t distance = Utils::get_cyclical_distance(
				mapped_page, target_page);
				if (distance > *max_dist)
				{
					*max_dist = distance;
					*max_dist_frame = frame;
					*max_dist_page = mapped_page;
				}
			}
		}
		return frame;
	}

	// Otherwise, continue traversal - as below...

	word_t max_frame = 0;
	uint64_t nonzero_targets = 0;

	for (uint64_t word_offset = 0; word_offset < PAGE_SIZE; ++word_offset)
	{
		// Reading the word from the page table
		const word_t target_frame = pa_frame_read_word(frame, word_offset);
		// target_frame is pointing to another frame, we continue down the tree
		if (ROOT_FRAME != target_frame) 
		{
			max_frame = traverse_page_table(
				target_frame, depth + 1, empty_table, target_page, max_dist, max_dist_frame, max_dist_page);

			// If the empty table indicator is set, it means one of the recursive
			// calls have found an empty page table. That page table is the one
			// returned by the recursive calls.
			if (*empty_table)
			{
				return max_frame;
			}

			// Otherwise, the maximum allocated frame is updated
			if (max_frame < target_frame)
			{
				max_frame = target_frame;
			}

			// Count the amount of non-zero targets in the current frame, will be
			// used later to determine whether the current frame is an empty page table.
			nonzero_targets++;
		}
		// target_frame is not pointing to any frame, continue to next word
	}

	// If all the entries in the current frame (the current part of the page table)
	// are empty, it means that this frame can be used for a new page.
	if (0 == nonzero_targets)
	{
		*empty_table = true;
		return frame;
	}

	return max_frame;
}

static uint64_t allocate_frame(uint64_t va)
{
	bool empty_table = false;
	uint64_t max_dist = 0;
	word_t max_dist_frame = 0;
	word_t max_dist_page = 0;

	// This call will retrieve the target frame for the new page to be allocated.
	// If the target frame is beyond the capacity of the RAM, the function will
	// use max_dist_frame to evict a page and allocate the new page in its place.
	// max_dist will not be used, as max_dist_frame has all the information required.
	// Note that the traversal starts from the root of the tree.
	const uint64_t target_frame = traverse_page_table(
		ROOT_FRAME, 0, &empty_table, Utils::va_get_page(va), 
		&max_dist, &max_dist_frame, &max_dist_page);

	// CASE 1: An empty page table, we will just reuse it (no overriding required)
	if (empty_table)
	{
		return target_frame;
	}

	// CASE 2: Locate unused frames in the RAM
	if (target_frame + 1 < NUM_FRAMES)
	{
		// Allocate the new page in the target frame
		zero_frame(target_frame + 1);
		return target_frame + 1;
	}

	// CASE 3: RAM is full, evicting a page is needed
	PMevict(max_dist_frame, max_dist_page);

	return max_dist_frame;
}

/**
 * \brief Locates the frame hosting a page referred by the given virtual address.
 *		  If the page is not found, a free frame will be found and allocated.
 *		  If no more pages are available, a page will be evicted, and reallocated
 *		  for the new page.
 * \return When the frame returned > NUM_FRAMES, then it means
 *		   that all pages are occupied.
 */
static bool locate_frame_from_va(uint64_t* free_frame)
{
	// TODO: Implement
	return false;
}

static uint64_t load_page_from_va(uint64_t va)
{
	// Traverse the tree to find the page


	

	// If the page is , we find a free frame to host the page.
	uint64_t free_frame = 0;
	bool is_existing = locate_frame_from_va(&free_frame);
	if (!is_existing && (NUM_FRAMES < free_frame))
	{
		// TODO: PLACEHOLDER CODE.
		return swap_page(va);
	}

	if (is_existing)
	{
		return free_frame;
	}

	return 0;
}

void VMinitialize()
{
	zero_frame(ROOT_FRAME);
}

int VMread(uint64_t virtualAddress, word_t* value)
{
	// This stores the address to the physical frame that contains the data
	// pointed by the given virtual address.
	const uint64_t pa_dest_frame = load_page_from_va(virtualAddress);

	*value = pa_frame_read_word(pa_dest_frame, Utils::va_get_offset(virtualAddress));

	return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value)
{
	const uint64_t pa_dest_frame = load_page_from_va(virtualAddress);

	pa_frame_write_word(pa_dest_frame, Utils::va_get_offset(virtualAddress), value);

	return 1;
}