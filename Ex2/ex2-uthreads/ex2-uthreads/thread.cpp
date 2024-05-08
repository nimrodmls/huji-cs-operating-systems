#include <signal.h>

#include "thread.h"

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
        : "=g" (ret)
        : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
        "rol    $0x9,%0\n"
        : "=g" (ret)
        : "0" (addr));
    return ret;
}

#endif

thread::thread(thread_id id, thread_entry_point ep) :
	id(id), env_blk(), stack(new char[STACK_SIZE]), state(READY), sleep_time(0), elapsed_quantums(0)
{
    // Initializes environment block to use the right stack,
    // and to run from the function 'entry_point',
    // when we'll use siglongjmp to jump into the thread.
    // * This code has been adapted from the code provided in the exercise *
    address_t sp = reinterpret_cast<address_t>(stack + STACK_SIZE - sizeof(address_t));
    address_t pc = reinterpret_cast<address_t>(ep);
    // return value omitted, as this is only initialization
    (void)sigsetjmp(env_blk, 1);
    (env_blk->__jmpbuf)[JB_SP] = translate_address(sp);
    (env_blk->__jmpbuf)[JB_PC] = translate_address(pc);
    (void)sigemptyset(&env_blk->__saved_mask);
}

thread::thread(thread_id id) :
    id(id), env_blk(), stack(), state(RUNNING), sleep_time(0), elapsed_quantums(1)
{
    // return value omitted, as this is only initialization
    (void)sigsetjmp(env_blk, 1);
}

void thread::run()
{
    state = RUNNING;
    // Starts the thread by jumping using the environment block
	// that was set when thread has been initialized/paused.
	siglongjmp(env_blk, RESUMED);
}

int thread::suspend(bool is_blocked)
{
    return sigsetjmp(env_blk, 1);
}
