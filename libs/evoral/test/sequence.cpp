#include <evoral/Sequence.hpp>

using namespace Evoral;

int
main()
{
	Glib::thread_init();

	Sequence s(100);
	return 0;
}
