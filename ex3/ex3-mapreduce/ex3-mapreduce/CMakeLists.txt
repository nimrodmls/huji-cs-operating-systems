# CMakeList.txt : CMake project for ex3-mapreduce, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (ex3-mapreduce 
				"ex3-mapreduce.cpp" 
				"MapReduceFramework.cpp" 
				"MapReduceClient.cpp" 
				"Thread.cpp"
				"Job.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ex3-mapreduce PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
