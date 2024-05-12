# CMakeList.txt : CMake project for ex1-measure, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (ex1-measure "measure.cpp" "memory_latency.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ex1-measure PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
