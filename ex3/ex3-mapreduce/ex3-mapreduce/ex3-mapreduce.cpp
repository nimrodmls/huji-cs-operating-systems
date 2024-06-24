// ex3-mapreduce.cpp : Defines the entry point for the application.
//
#include <atomic>
#include <iostream>
#include "MapReduceClient.h"

uint8_t _get_stage(uint64_t var)
{
	// The stage ID is stored in the 2 most significant bits of the counter
	return var >> 62;
}

uint32_t _get_stage_processed(uint64_t var)
{
	// The processed count is stored in the 31 "middle" bits of the counter
	// (between the stage ID and the total count)
	return (var << 33) >> 33;
}

uint32_t _get_stage_total(uint64_t var)
{
	// The total count is stored in the 31 least significant bits of the counter
	// (after the processed count)
	return (var >> 33) & 0x7FFFULL;
}

void set_stage(uint64_t& var, uint8_t stage)
{
	// Clear the stage bits
	var &= ~(0x3ULL << 62);
	// Set the new stage bits
	var |= (static_cast<uint64_t>(stage) << 62);
}

void increment_processed(uint64_t& var)
{
	// Increment the processed count
	var += 0x1ULL;
}

void set_total(uint64_t& var, uint32_t total)
{
	// Clear the total count bits
	var &= ~(0x7FFFFFFFULL);
	// Set the new total count bits
	var |= (static_cast<uint64_t>(total) << 33);
}

class KChar : public K2 {
public:
	KChar(char c) : c(c) { }
	virtual bool operator<(const K2& other) const {
		return c < static_cast<const KChar&>(other).c;
	}
	char c;
};

class VCount : public V2 {
public:
	VCount(int count) : count(count) { }
	int count;
};

int main()
{
	std::atomic<bool> a = true;
	bool b = false;
	a.compare_exchange_strong(b, true);
	std::cout << a << std::endl;
	uint64_t test = 0;
	set_stage(test, 1);
	set_total(test, 10);
	increment_processed(test);
	increment_processed(test);
	increment_processed(test);
	std::cout << "Stage: " << static_cast<uint32_t>(_get_stage(test)) << std::endl;
	std::cout << "Processed: " << _get_stage_processed(test) << std::endl;
	std::cout << "Total: " << _get_stage_total(test) << std::endl;

	std::vector<std::pair<KChar, VCount>> iv;
	iv.emplace_back(std::make_pair<>('c', 50));
	iv.emplace_back(std::make_pair<>('a', 60));
	iv.emplace_back(std::make_pair<>('k', 20));
	iv.emplace_back(std::make_pair<>('q', 10));
	std::sort(iv.begin(), iv.end(), [](const std::pair<KChar, VCount>& a1, const std::pair<KChar, VCount>& a2)
	{
			return a1.first < a2.first;
	});
	auto m = std::minmax_element(iv.begin(), iv.end(), [](const std::pair<KChar, VCount>& a1, const std::pair<KChar, VCount>& a2)
		{
			return a1.first < a2.first;
		});
	std::cout << static_cast<uint32_t>(m.second->first.c) << std::endl;
	return 0;
}
