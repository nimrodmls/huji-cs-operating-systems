#ifndef COMMON_H
#define COMMON_H

#include <iostream>

#include "MapReduceClient.h"

namespace Common
{
	inline void emit_system_error(std::string message)
	{
		std::cout << "system error: " << message << std::endl;
		exit(1);
	}

	inline bool key_less_than(IntermediatePair& p1, IntermediatePair& p2)
	{
		return *p1.first < *p2.first;
	}

	inline bool key_equals(IntermediatePair& p1, IntermediatePair& p2)
	{
		return !key_less_than(p1, p2) && !key_less_than(p2, p1);
	}

}

#endif // COMMON_H