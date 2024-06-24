#ifndef COMMON_H
#define COMMON_H

#include <iostream>

namespace Common
{
	inline void emit_system_error(std::string message)
	{
		std::cout << "system error: " << message << std::endl;
		exit(1);
	}
}

#endif // COMMON_H