#ifndef UTILS_H
#define UTILS_H

#include <algorithm>

#include "MemoryConstants.h"

#define PAGE_INDEX_WIDTH (VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH)

namespace Utils
{
	/**
	 * \brief Returns the index to the page table in the given virtual address
	 *		  at the specified depth.
	 * \param va The virtual address.
	 * \param depth The depth from which to extract the index.
	 * \note For example, when given va=011011 with OFFSET_WIDTH=1, '1' is the
	 *		 offset and '01101' is the page index. For depth=i, the function will
	 *		 extract the i'th index from the left. So for depth=0 the function
	 *		 returns 0, and for depth=1 the function returns 1, and so on.
	 */
	constexpr uint64_t va_get_page_table_index(uint64_t va, uint64_t depth)
	{
		// The first part is for calculating the page index up until the specified
		// depth. And the last logical and is for extracting the index at the
		// the needed depth (as it's guaranteed to be the LSB).
		return (va >> (OFFSET_WIDTH + PAGE_INDEX_WIDTH - (OFFSET_WIDTH * (depth + 1)))) & 
			   ((1ULL << OFFSET_WIDTH) - 1);
	}

	/**
	 * \brief Returns the offset to the page in the given virtual address.
	 * \param va The virtual address.
	 * \note See the note in va_get_page_index_depth for an example.
	 */
	constexpr uint64_t va_get_offset(uint64_t va)
	{
		//return va & ((1ULL << OFFSET_WIDTH) - 1);
		return va_get_page_table_index(va, TABLES_DEPTH);
	}

	constexpr uint64_t va_get_page(uint64_t va)
	{
		return va >> OFFSET_WIDTH;
	}

}

#endif // UTILS_H