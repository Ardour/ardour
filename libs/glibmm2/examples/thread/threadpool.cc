
#include <iostream>
#include <glibmm/random.h>
#include <glibmm/thread.h>
#include <glibmm/threadpool.h>
#include <glibmm/timer.h>


namespace
{

Glib::StaticMutex mutex = GLIBMM_STATIC_MUTEX_INIT;

void print_char(char c)
{
  Glib::Rand rand;

  for(int i = 0; i < 100; ++i)
  {
    {
      Glib::Mutex::Lock lock (mutex);
      std::cout << c;
      std::cout.flush();
    }
    Glib::usleep(rand.get_int_range(10000, 100000));
  }
}

} // anonymous namespace


int main(int, char**)
{
  Glib::thread_init();

  Glib::ThreadPool pool (10);

  for(char c = 'a'; c <= 'z'; ++c)
  {
    pool.push(sigc::bind<1>(sigc::ptr_fun(&print_char), c));
  }
  
  pool.shutdown();

  std::cout << std::endl;

  return 0;
}

