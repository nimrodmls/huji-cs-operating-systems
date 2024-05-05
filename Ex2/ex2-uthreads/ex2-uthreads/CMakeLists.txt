# CMakeList.txt : CMake project for ex2-uthreads, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (ex2-uthreads "main.cpp"  "uthreads.cpp" "uthread_manager.cpp" "uthread_manager.h" "thread.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ex2-uthreads PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
