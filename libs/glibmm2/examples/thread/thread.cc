
#include <iostream>
#include <queue>
#include <glibmm/random.h>
#include <glibmm/thread.h>
#include <glibmm/timer.h>


namespace
{

class MessageQueue : public sigc::trackable
{
public:
  MessageQueue();
  ~MessageQueue();

  void producer();
  void consumer();

private:
  Glib::Mutex     mutex_;
  Glib::Cond      cond_push_;
  Glib::Cond      cond_pop_;
  std::queue<int> queue_;
};


MessageQueue::MessageQueue()
{}

MessageQueue::~MessageQueue()
{}

void MessageQueue::producer()
{
  Glib::Rand rand (1234);

  for(int i = 0; i < 200; ++i)
  {
    {
      Glib::Mutex::Lock lock (mutex_);

      while(queue_.size() >= 64)
        cond_pop_.wait(mutex_);

      queue_.push(i);
      std::cout << '*';
      std::cout.flush();

      cond_push_.signal();
    }

    if(rand.get_bool())
      continue;

    Glib::usleep(rand.get_int_range(0, 100000));
  }
}

void MessageQueue::consumer()
{
  Glib::Rand rand (4567);

  for(;;)
  {
    {
      Glib::Mutex::Lock lock (mutex_);

      while(queue_.empty())
        cond_push_.wait(mutex_);

      const int i = queue_.front();
      queue_.pop();
      std::cout << "\x08 \x08";
      std::cout.flush();

      cond_pop_.signal();

      if(i >= 199)
        break;
    }

    if(rand.get_bool())
      continue;

    Glib::usleep(rand.get_int_range(10000, 200000));
  }
}

}


int main(int, char**)
{
  Glib::thread_init();

  MessageQueue queue;

  Glib::Thread *const producer = Glib::Thread::create(
      sigc::mem_fun(queue, &MessageQueue::producer), true);

  Glib::Thread *const consumer = Glib::Thread::create(
      sigc::mem_fun(queue, &MessageQueue::consumer), true);

  producer->join();
  consumer->join();

  std::cout << std::endl;

  return 0;
}

