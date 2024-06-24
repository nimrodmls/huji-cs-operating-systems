# CMakeList.txt : CMake project for ex3-mapreduce, include source and define
# project specific logic here.
#

find_package(Threads REQUIRED)

# Add source to this project's executable.
add_executable (ex3-mapreduce 
				"ex3-mapreduce.cpp" 
				"MapReduceFramework.cpp" 
				"MapReduceClient.cpp" 
				"Thread.cpp"
				"JobContext.cpp"
				"Barrier.cpp"
				"Semaphore.cpp" "Mutex.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ex3-mapreduce PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(ex3-mapreduce ${CMAKE_THREAD_LIBS_INIT})
# TODO: Add tests and install targets if needed.
