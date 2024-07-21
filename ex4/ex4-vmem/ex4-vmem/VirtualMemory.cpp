#include <algorithm>

#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include "Utils.h"

/**
 * NOTE: It's a global convention in this project to use 'pa' as an
 *		 abbreviation for 'physical address' and 'va' as an abbreviation for
 *		 'virtual address'.
 */

 // Return statuses
typedef enum
{
	VM_FAILURE = 0,
	VM_SUCCESS = 1

} VMStatus;

// The index of the frame for the page table tree root
// (kept in physical memory at all times)
constexpr uint64_t ROOT_FRAME = 0;

/**
 * \brief Reading a single word from the given frame, at the given offset.
 * \param frame The target frame to read the word from
 * \param offset Offset to the selected frame to read the word from
 * \return The word read from the frame
 */
static word_t pa_frame_read_word(uint64_t frame, uint64_t offset)
{
	word_t word = 0;
	PMread((frame * PAGE_SIZE) + offset, &word);
	return word;
}

/**
 * \brief Writing a single word to the given frame, at the given offset.
 * \param frame The target frame to write the word to
 * \param offset Offset to the selected frame to write the word to
 * \param value The value to write to the frame
 */
static void pa_frame_write_word(uint64_t frame, uint64_t offset, word_t value)
{
	PMwrite((frame * PAGE_SIZE) + offset, value);
}

/**
 * \brief Zeros out the given frame.
 * \param frame The target frame
 */
static void pa_zero_frame(uint64_t frame)
{
	for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset)
	{
		pa_frame_write_word(frame, offset, 0);
	}
}

/**
 * \brief Calculates the cyclical distance between two pages.
 * \param mapped_page The page that is currently mapped
 * \param target_page The target page to calculate the distance to
 * \return The cyclical distance
 */
uint64_t get_cyclical_distance(
	uint64_t mapped_page, uint64_t target_page)
{
	const uint64_t dist = std::abs(static_cast<int64_t>(target_page - mapped_page));
	return std::min(static_cast<uint64_t>(NUM_PAGES - dist), dist);
}

/**
 * \brief Traversing the page table tree (DFS).
 *		  See the wrapper function for more information.
 * \param host_page_table_frame See docs at the wrapper function
 * \param target_page See docs at the wrapper function
 * \param frame The frame to traverse from (this is updated during the DFS traversal)
 * \param branch_route The route taken to reach the current frame
 * \param depth The current depth in the page table tree
 * \param empty_table See docs at the wrapper function
 * \param max_dist See docs at the wrapper function
 * \param max_dist_frame See docs at the wrapper function
 * \param max_dist_page See docs at the wrapper function
 * \param max_dist_page_table See docs at the wrapper function
 * \return See docs at the wrapper functions
 */
static word_t traverse_page_table(
	word_t host_page_table_frame,
	uint64_t target_page,
	word_t frame,
	word_t branch_route,
	uint64_t depth, 
	bool* empty_table,
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
			// Updating the branching route to include the current word offset
			const word_t current_branch = (branch_route << OFFSET_WIDTH) + word_offset;

			if (TABLES_DEPTH == depth + 1) // The other frame is a storing a page
			{
				const uint64_t distance = get_cyclical_distance(
					current_branch, target_page);
				if (distance > *max_dist)
				{
					*max_dist = distance;
					*max_dist_frame = target_frame;
					*max_dist_page = current_branch;
					*max_dist_page_table = frame;
				}
			}
			else // The other frame is storing a page table
			{
				const word_t candidate_frame = traverse_page_table(
					host_page_table_frame, target_page, target_frame, 
					current_branch, depth + 1, empty_table, max_dist, max_dist_frame, 
					max_dist_page, max_dist_page_table);

				// If the empty table indicator is set, it means one of the recursive
				// calls have found an empty page table. That page table is the one
				// returned by the recursive calls.
				if (*empty_table)
				{
					// Unlinking the empty page table from the current page table
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
 * \brief Traversing the page table tree to find all the relevant information
 *		  regarding the target page.
 * \param host_page_table_frame The frame of the page table that will be hosting
 *								the target page (or other parent page tables of the target page
 *								This is used to determine whether a page table can be reused,
 *								as it is possible that the host page table frame is an
 *								empty page table.
 * \param target_page The target page to find in the page table tree
 * \param empty_table Whether an empty page table was encoutered while traversing.
 * \param max_dist The maximum distance found between the target page and any other page.
 * \param max_dist_frame The frame hosting the page that is the furthest from the target page.
 * \param max_dist_page The page that is the furthest from the target page.
 * \param max_dist_page_table The page table that is hosting the page that is the
 *							  furthest from the target page.
 * \return The maximum frame found in the page table tree.
 *		   If this value +1 is beyond the capacity of the RAM then the memory
 *		   is completely full. In this case, either empty_table is true,
 *		   and that means there is an empty table which can be reused
 *		   and so the returned value is the frame of the empty table
 *		   (the said frame will be unlinked from the page table tree).
 *		   It is also possible that empty_table is false, in this case
 *		   the max_dist parameters can be used to determine which
 *		   page will be evicted to make room for the new page.
 * \note This is a wrapper to the recursive function traverse_page_table.
 *       Note that all the max_dist parameters are complementary to each other,
 *		 and are intended to be used in unison.
 */
static word_t traverse_page_table(
	word_t host_page_table_frame,
	uint64_t target_page,
	bool* empty_table,
	uint64_t* max_dist,
	word_t* max_dist_frame,
	word_t* max_dist_page,
	word_t* max_dist_page_table)
{
	return traverse_page_table(
		host_page_table_frame, target_page, 
		ROOT_FRAME, // Starting the traversal from the root
		0, // Branching route is 0 at the beginning
		0, // Depth is 0 at the beginning
		empty_table, max_dist, max_dist_frame, max_dist_page, max_dist_page_table);
}

/**
 * \brief Allocates a new frame. If the RAM is full, a page will be evicted,
 *        the decision of which page is evicted is based on the page that will
 *		  be essentially loaded into the physical memory.
 * \param target_page The page intended to be loaded into the physical memory
 *					  This function does not load it, but this information is
 *					  necessary to determine page eviction policy.
 * \param host_page_table The page table that will be hosting the target page
 * \return The frame index of the allocated frame.
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
		host_page_table, target_page, &empty_table, &max_dist,
		&max_dist_frame, &max_dist_page, &max_dist_page_table);

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
		Utils::page_get_index_depth(max_dist_page, TABLES_DEPTH - 1),
		ROOT_FRAME);
	pa_zero_frame(max_dist_frame);

	return max_dist_frame;
}

/**
 * \brief Loads a page from the pagefile to the RAM.
 *		  If the page is already in the RAM, it will not be loaded again.
 * \param page The page to load to the physical memory
 * \return The frame index to the page in the RAM.
 */
static uint64_t load_page(word_t page)
{
	bool is_new_page = false;
	word_t ancestor_node = ROOT_FRAME;

	// Iterating on the page table tree according to the given virtual address
	for (uint64_t depth = 0; depth < TABLES_DEPTH; ++depth)
	{
		const uint64_t page_index = Utils::page_get_index_depth(page, depth);
		word_t target_frame = pa_frame_read_word(ancestor_node, page_index);

		if (ROOT_FRAME == target_frame) // If the page table is not mapped, mapping it
		{
			target_frame = allocate_frame(page, ancestor_node);
			pa_frame_write_word(ancestor_node, page_index, target_frame);

			// Marking the page as newly allocated, this will be used to determine
			// whether the page should be restored from the pagefile.
			is_new_page = true;
		}

		ancestor_node = target_frame;
	}

	// If the page is new, it has to be swapped into the newly allocated frame
	if (is_new_page)
	{
		PMrestore(ancestor_node, page);
	}

	return ancestor_node;
}

// See VirtualMemory.h for documentation
void VMinitialize()
{
	pa_zero_frame(ROOT_FRAME);
}

// See VirtualMemory.h for documentation
int VMread(uint64_t virtualAddress, word_t* value)
{
	if (VIRTUAL_MEMORY_SIZE <= virtualAddress)
	{
		return VMStatus::VM_FAILURE;
	}

	// This stores the address to the physical frame that contains the data
	// pointed by the given virtual address.
	const uint64_t pa_dest_frame = load_page(
		static_cast<word_t>(Utils::va_get_page(virtualAddress)));

	*value = pa_frame_read_word(pa_dest_frame, Utils::va_get_offset(virtualAddress));

	return VMStatus::VM_SUCCESS;
}

// See VirtualMemory.h for documentation
int VMwrite(uint64_t virtualAddress, word_t value)
{
	if (VIRTUAL_MEMORY_SIZE <= virtualAddress)
	{
		return VMStatus::VM_FAILURE;
	}

	const uint64_t pa_dest_frame = load_page(
		static_cast<word_t>(Utils::va_get_page(virtualAddress)));

	pa_frame_write_word(pa_dest_frame, Utils::va_get_offset(virtualAddress), value);

	return VMStatus::VM_SUCCESS;
}