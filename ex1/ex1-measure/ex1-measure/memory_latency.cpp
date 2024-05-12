// OS 24 EX1

#include "memory_latency.h"

#include <cmath>
#include <iostream>

#include "measure.h"

#define INITIAL_SIZE 100
#define DECIMAL_BASE 10
#define GALOIS_POLYNOMIAL ((1ULL << 63) | (1ULL << 62) | (1ULL << 60) | (1ULL << 59))

typedef enum _program_args
{
	ARG_MAX_SIZE = 1,
	ARG_FACTOR,
	ARG_REPEAT,
    NUM_OF_ARGS

} program_args;

typedef enum _program_status
{
	STATUS_FAILURE = -1,
	STATUS_SUCCESS

} program_status;

/**
 * Converts the struct timespec to time in nano-seconds.
 * @param t - the struct timespec to convert.
 * @return - the value of time in nano-seconds.
 */
uint64_t nanosectime(struct timespec t)
{
    return t.tv_nsec + (t.tv_sec * 1000000000ull);
}

/**
* Measures the average latency of accessing a given array in a sequential order.
* @param repeat - the number of times to repeat the measurement for and average on.
* @param arr - an allocated (not empty) array to preform measurement on.
* @param arr_size - the length of the array arr.
* @param zero - a variable containing zero in a way that the compiler doesn't "know" it in compilation time.
* @return struct measurement containing the measurement with the following fields:
*      double baseline - the average time (ns) taken to preform the measured operation without memory access.
*      double access_time - the average time (ns) taken to preform the measured operation with memory access.
*      uint64_t rnd - the variable used to randomly access the array, returned to prevent compiler optimizations.
*/
struct measurement measure_sequential_latency(uint64_t repeat, array_element_t* arr, uint64_t arr_size, uint64_t zero)
{
    repeat = arr_size > repeat ? arr_size : repeat; // Make sure repeat >= arr_size

    // Baseline measurement:
    struct timespec t0;
    timespec_get(&t0, TIME_UTC);
    register uint64_t rnd = 12345;
    for (register uint64_t i = 0; i < repeat; i++)
    {
        register uint64_t index = i % arr_size;
        rnd ^= (index + zero) & zero;
        rnd = (rnd >> 1) ^ ((0 - (rnd & 1)) & GALOIS_POLYNOMIAL);  // Advance rnd pseudo-randomly (using Galois LFSR)
    }
    struct timespec t1;
    timespec_get(&t1, TIME_UTC);

    // Memory access measurement:
    struct timespec t2;
    timespec_get(&t2, TIME_UTC);
    rnd = (rnd & zero) ^ 12345;
    for (register uint64_t i = 0; i < repeat; i++)
    {
        register uint64_t index = i % arr_size;
        rnd ^= arr[index + zero] & zero;
        rnd = (rnd >> 1) ^ ((0 - (rnd & 1)) & GALOIS_POLYNOMIAL);  // Advance rnd pseudo-randomly (using Galois LFSR)
    }
    struct timespec t3;
    timespec_get(&t3, TIME_UTC);

    // Calculate baseline and memory access times:
    double baseline_per_cycle = (double)(nanosectime(t1) - nanosectime(t0)) / (repeat);
    double memory_per_cycle = (double)(nanosectime(t3) - nanosectime(t2)) / (repeat);
    struct measurement result;

    result.baseline = baseline_per_cycle;
    result.access_time = memory_per_cycle;
    result.rnd = rnd;
    return result;
}

void print_measurement_results(
    int size, const measurement* random_result, const measurement* sequential_result)
{
	std::cout << size << ","
			  << random_result->access_time - random_result->baseline << ","
			  << sequential_result->access_time - sequential_result->baseline << std::endl;
}

/**
 * Runs the logic of the memory_latency program. Measures the access latency for random and sequential memory access
 * patterns.
 * Usage: './memory_latency max_size factor repeat' where:
 *      - max_size - the maximum size in bytes of the array to measure access latency for.
 *      - factor - the factor in the geometric series representing the array sizes to check.
 *      - repeat - the number of times each measurement should be repeated for and averaged on.
 * The program will print output to stdout in the following format:
 *      mem_size_1,offset_1,offset_sequential_1
 *      mem_size_2,offset_2,offset_sequential_2
 *              ...
 *              ...
 *              ...
 */
int main(int argc, char* argv[])
{
    if (NUM_OF_ARGS != argc)
    {
	    std::cerr << "Invalid number of arguments" << std::endl;
		return STATUS_FAILURE;
	}

    // Converting and validating the arguments. We don't check errno since
    // the user input should not be 0 or negative for any of the inputs.
    const int max_size = strtol(argv[ARG_MAX_SIZE], nullptr, DECIMAL_BASE);
    if (0 >= max_size)
    {
		std::cerr << "Invalid max_size argument" << std::endl;
		return STATUS_FAILURE;
    }

    const double factor = strtod(argv[ARG_FACTOR], nullptr);
    if (0.0 >= factor)
    {
        std::cerr << "Invalid factor argument" << std::endl;
        return STATUS_FAILURE;
    }

    const int repeat = strtol(argv[ARG_REPEAT], nullptr, DECIMAL_BASE);
    if (0 >= repeat)
    {
        std::cerr << "Invalid repeat argument" << std::endl;
        return STATUS_FAILURE;
    }

    // zero==0, but the compiler doesn't know it. Use as the zero arg of measure_latency and measure_sequential_latency.
    struct timespec t_dummy;
    timespec_get(&t_dummy, TIME_UTC);
    const uint64_t zero = nanosectime(t_dummy)>1000000000ull?0:nanosectime(t_dummy);

    int current_size = INITIAL_SIZE;
    while (max_size > current_size)
    {
        void * data = malloc(current_size);
        if (nullptr == data)
        {
            std::cerr << "Failed to allocate memory" << std::endl;
			return STATUS_FAILURE;
        }

        measurement random_result = { 0 };
        random_result = measure_latency(
            repeat, 
            static_cast<array_element_t*>(data), 
            current_size / sizeof(array_element_t),
            zero);
        measurement sequential_result = { 0 };
        sequential_result = measure_sequential_latency(
            repeat, 
            static_cast<array_element_t*>(data), 
            current_size / sizeof(array_element_t),
            zero);
        print_measurement_results(current_size, &random_result, &sequential_result);

        free(data);

        // Getting the next array size to measure, rounding up to the nearest integer.
        current_size = static_cast<int>(std::ceil(current_size * factor));
    }

    return STATUS_SUCCESS;
}