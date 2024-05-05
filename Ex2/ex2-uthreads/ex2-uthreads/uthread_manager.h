#ifndef	UTHREAD_MANAGER_H
#define UTHREAD_MANAGER_H

#include <map>
#include <memory>
#include <queue>

#include "thread.h"
#include "uthreads.h"

class uthread_exception : public std::exception
{};

class uthread_manager
{
public:
	explicit uthread_manager(int quantum_usecs);
	~uthread_manager() = default;
	uthread_manager(const uthread_manager&) = delete;
	uthread_manager operator=(const uthread_manager&) = delete;

	void spawn(thread_entry_point entry_point);

private:
	int quantum_usecs_interval;
	std::vector<thread_id> sleeper_threads;
	std::queue<thread_id> ready_threads;
	std::map<thread_id, std::shared_ptr<thread>> threads;
};

class global_uthread_manager
{
public:
	static void init(int quantum_usecs);
	static std::shared_ptr<uthread_manager> get_instance();

private:
	static std::shared_ptr<uthread_manager> get_instance(int quantum_usecs);
};

#endif // UTHREAD_MANAGER_H