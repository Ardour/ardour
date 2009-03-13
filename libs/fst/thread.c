#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <windows.h>

typedef struct {
    void* (*thread_function)(void*);
    void* thread_arg;
    pthread_t thread_id;
    pthread_mutex_t init_lock;
    pthread_cond_t init_cond;
    pthread_attr_t attr;
} real_thread_info_t;

static DWORD WINAPI
fake_thread_proxy (LPVOID parameter) 
{
	real_thread_info_t* rti = (real_thread_info_t*) parameter;

	fprintf (stderr, "WINDOWS THREAD, @ pthread = %p\n", pthread_self());

	pthread_mutex_lock (&rti->init_lock);
	rti->thread_id = pthread_self();
	pthread_cond_signal (&rti->init_cond);
	pthread_mutex_unlock (&rti->init_lock);

#if 0
	if (pthread_attr_get_schedparam (&rti->attr)) {
		pthread_set_schedparam (pthread_self(), policy, sched_param);
	}
#endif
	/* XXX no way to use pthread API to set contention scope,
	   because that has to be done before a thread is created.
	   But ... its only meaningful for an M:N thread implemenation
	   so its not important for the only platform where
	   this code matters (Linux running Wine) because Linux
	   uses a 1:1 thread design.
	*/

	return (DWORD) rti->thread_function (rti->thread_arg);
}

int
wine_pthread_create (pthread_t* thread_id, const pthread_attr_t* attr, void *(*function)(void*), void* arg)
{
	DWORD tid;
	size_t stack_size;

	fprintf (stderr, "****** Lets make a windows pthread\n");

	real_thread_info_t* rti = (real_thread_info_t*) malloc (sizeof (real_thread_info_t));

	rti->thread_function = function;
	rti->thread_arg = arg;
	if (attr) {
		rti->attr = *attr;
	}

	fprintf (stderr, "\tset up the locks\n");

	pthread_mutex_init (&rti->init_lock, NULL);
	pthread_cond_init (&rti->init_cond, NULL);
	
	pthread_mutex_lock (&rti->init_lock);

	fprintf (stderr, "\tget the stacksize\n");

	if (attr) {
		if (pthread_attr_getstacksize (attr, &stack_size) != 0) {
			stack_size = 0;
		}
	} else {
		stack_size = 0;
	}

	fprintf (stderr, "\tget that sucker started in the proxy, stacksize = %u\n", stack_size);

	if (CreateThread (0, stack_size, fake_thread_proxy, rti, 0, &tid) == NULL) {
		return -1;
	}

	pthread_cond_wait (&rti->init_cond, &rti->init_lock);
	pthread_mutex_unlock (&rti->init_lock);
	fprintf (stderr, "\tlet it run\n");

	*thread_id = rti->thread_id;

	return 0;
} 
