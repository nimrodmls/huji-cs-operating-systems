#ifndef	UTHREAD_MANAGER_H
#define UTHREAD_MANAGER_H

#include <memory>

class uthread_exception : public std::exception
{};

class uthread_manager
{
public:
	uthread_manager(const uthread_manager&) = delete;
	uthread_manager operator=(const uthread_manager&) = delete;

	static void init(int quantum_usecs);
	static std::shared_ptr<uthread_manager> get_instance();

	

private:
	explicit uthread_manager(int quantum_usecs);
	~uthread_manager() = default;

	int quantum_usecs_interval;
	// sleeper tids+time list
	// ready tids queue
	// tid-env map


	static std::shared_ptr<uthread_manager> instance;
};

#endif // UTHREAD_MANAGER_H