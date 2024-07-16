﻿#include "VirtualMemory.h"
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

static void pa_zero_frame(uint64_t frame)
{
	for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset)
	{
		pa_frame_write_word(frame, offset, 0);
	}
}

uint64_t get_cyclical_distance(
	uint64_t mapped_page, uint64_t target_page)
{
	const uint64_t dist = std::abs(static_cast<int64_t>(target_page - mapped_page));
	return std::min(static_cast<uint64_t>(NUM_PAGES - dist), dist);
}

/**
 * \brief Traversing the page table from the given frame node.
 *		  This is essentially an implementation of DFS.
 * \param va 
 * \param depth 
 */
static word_t traverse_page_table(
	word_t frame,
	word_t current_va,
	uint64_t depth, 
	bool* empty_table,
	word_t host_page_table_frame,
	uint64_t target_page,
	uint64_t* max_dist, 
	word_t* max_dist_frame,
	word_t* max_dist_page,
	word_t* max_dist_page_table)
{
	word_t max_frame = 0; // Stores the maximum frame found in the current page table
	uint64_t nonzero_targets = 0;

	for (word_t word_offset = 0; word_offset < PAGE_SIZE; ++word_offset)
	{
		// Reading the word from the page table
		word_t target_frame = pa_frame_read_word(frame, word_offset);
		// target_frame is pointing to another frame, we continue down the tree
		if (ROOT_FRAME != target_frame) 
		{
			// TODO: Document this
			word_t cur_va = (current_va << OFFSET_WIDTH) + word_offset;

			// The other frame is a storing a page
			if (TABLES_DEPTH == depth + 1)
			{
				const uint64_t distance = get_cyclical_distance(
					cur_va, target_page);
				if (distance > *max_dist)
				{
					*max_dist = distance;
					*max_dist_frame = target_frame;
					*max_dist_page = cur_va;
					*max_dist_page_table = frame;
				}
			}
			else // The other frame is storing a page table
			{
				word_t candidate_frame = traverse_page_table(
				   target_frame, cur_va, depth + 1, empty_table, host_page_table_frame, 
				   target_page, max_dist, max_dist_frame, max_dist_page, max_dist_page_table);

				// If the empty table indicator is set, it means one of the recursive
				// calls have found an empty page table. That page table is the one
				// returned by the recursive calls.
				if (*empty_table)
				{
					if (candidate_frame == target_frame)
					{
						pa_frame_write_word(frame, word_offset, ROOT_FRAME);
					}
					return candidate_frame;
				}

				target_frame = candidate_frame;
			}

			// Updating the max frame spotted in the page tables
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
	// are empty, it means that this frame can be used for a new page. But only if
	// this frame is not the host page table frame (so new allocations won't be using
	// their own page table as the target frame).
	if ((0 == nonzero_targets) && 
		(host_page_table_frame != frame))
	{
		*empty_table = true;
		return frame;
	}

	// Taking either the maximum frame in the page tables referred to from the current
	// page, or taking the frame itself if it's the maximum frame.
	return std::max(max_frame, frame);
}

/**
 * \brief Allocates a new frame. If the RAM is full, a page will be evicted,
 *        the decision of which page is evicted is based on the page that will
 *		  be essentially loaded into the physical memory.
 * \param target_page 
 * \return 
 */
static word_t allocate_frame(word_t target_page, word_t host_page_table)
{
	bool empty_table = false;
	uint64_t max_dist = 0;
	word_t max_dist_frame = 0;
	word_t max_dist_page = 0;
	word_t max_dist_page_table = 0;

	// This call will retrieve the target frame for the new page to be allocated.
	// If the target frame is beyond the capacity of the RAM, the function will
	// use max_dist_frame to evict a page and allocate the new page in its place.
	// max_dist will not be used, as max_dist_frame has all the information required.
	// Note that the traversal starts from the root of the tree.
	const word_t target_frame = traverse_page_table(
		ROOT_FRAME, 0, 0, &empty_table, host_page_table, target_page,
		&max_dist, &max_dist_frame, &max_dist_page, &max_dist_page_table);

	// CASE 1: An empty page table, we will just reuse it (no overriding required)
	if (empty_table)
	{
		return target_frame;
	}

	// CASE 2: Locate unused frames in the RAM
	if (target_frame + 1 < NUM_FRAMES)
	{
		pa_zero_frame(target_frame + 1);
		return target_frame + 1;
	}

	// CASE 3: RAM is full, evicting a page is needed
	PMevict(max_dist_frame, max_dist_page);
	// Resetting the page table entry pointing to the evicted page
	pa_frame_write_word(
		max_dist_page_table, 
		//Utils::page_get_index_depth(target_page, TABLES_DEPTH), 
		Utils::page_get_index_depth(max_dist_page, TABLES_DEPTH - 1),
		ROOT_FRAME);
	pa_zero_frame(max_dist_frame);

	return max_dist_frame;
}

// TODO: Change va to page
/**
 * \brief Loads a page from the pagefile to the RAM.
 *		  If the page is already in the RAM, it will not be loaded again.
 * \param va Virtual address of the page to load.
 * \return The frame index to the page in the RAM.
 */
static uint64_t load_page(uint64_t va)
{
	bool is_new_page = false;
	word_t ancestor_node = ROOT_FRAME;

	// Iterating on the page table tree according to the given virtual address
	for (uint64_t depth = 0; depth < TABLES_DEPTH; ++depth)
	{
		const uint64_t page_index = Utils::va_get_page_table_index(va, depth);
		word_t target_frame = pa_frame_read_word(ancestor_node, page_index);

		if (ROOT_FRAME == target_frame) // If the page table is not mapped, mapping it
		{
			target_frame = allocate_frame(
				static_cast<word_t>(Utils::va_get_page(va)),
				ancestor_node);
			pa_frame_write_word(ancestor_node, page_index, target_frame);

			// Marking the page as newly allocated, this will be used to determine
			// whether the page should be restored from the pagefile.
			is_new_page = true;
		}

		ancestor_node = target_frame;
	}

	if (is_new_page)
	{
		PMrestore(ancestor_node, static_cast<word_t>(Utils::va_get_page(va)));
	}

	return ancestor_node;
}

void VMinitialize()
{
	pa_zero_frame(ROOT_FRAME);
}

int VMread(uint64_t virtualAddress, word_t* value)
{
	// This stores the address to the physical frame that contains the data
	// pointed by the given virtual address.
	const uint64_t pa_dest_frame = load_page(virtualAddress);

	*value = pa_frame_read_word(pa_dest_frame, Utils::va_get_offset(virtualAddress));

	return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value)
{
	const uint64_t pa_dest_frame = load_page(virtualAddress);

	pa_frame_write_word(pa_dest_frame, Utils::va_get_offset(virtualAddress), value);

	return 1;
}