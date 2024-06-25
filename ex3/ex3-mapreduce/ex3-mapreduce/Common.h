#ifndef COMMON_H
#define COMMON_H

#include <iostream>

#include "MapReduceClient.h"
#include "MapReduceFramework.h"

namespace Common
{
	inline void emit_system_error(std::string message)
	{
		std::cout << "system error: " << message << std::endl;
		exit(1);
	}

	inline bool key_less_than(const IntermediatePair& p1, const IntermediatePair& p2)
	{
		return *p1.first < *p2.first;
	}

	inline bool key_equals(const IntermediatePair& p1, const IntermediatePair& p2)
	{
		return !key_less_than(p1, p2) && !key_less_than(p2, p1);
	}

	inline uint32_t get_stage_processed(uint64_t state)
	{
		// The processed count is stored in the 31 least significant bits of the counter
		// (after the total count)
		return (state << 33) >> 33;
	}

	inline uint32_t get_stage_total(uint64_t state)
	{
		// The total count is stored in the 31 "middle" bits of the counter
		// (between the stage ID and the processed count)
		return (state & (0xFFFFFFFFFFFFFFFF >> 2)) >> 33;
	}

	inline stage_t get_stage(uint64_t state)
	{
		// The stage ID is stored in the 2 most significant bits of the counter
		return static_cast<stage_t>(state >> 62);
	}
}

#endif // COMMON_H