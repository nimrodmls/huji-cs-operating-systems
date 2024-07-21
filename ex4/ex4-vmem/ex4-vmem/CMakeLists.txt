# CMakeList.txt : CMake project for ex4-vmem, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (ex4-vmem
				"PhysicalMemory.cpp"
				"VirtualMemory.cpp"
				"SimpleTest.cpp"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ex4-vmem PROPERTY CXX_STANDARD 11)
endif()

# TODO: Add tests and install targets if needed.
