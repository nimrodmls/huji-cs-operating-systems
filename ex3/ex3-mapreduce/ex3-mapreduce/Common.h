#include <iostream>

inline void emit_system_error(std::string message)
{
	std::cout << "system error: " << message << std::endl;
	exit(1);
}