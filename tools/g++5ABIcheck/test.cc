#include <cstdio>
#include <string>

void check (std::string const& str) {
	std::printf ("%s\n",	str.c_str());
}

int main (int argc, char **argv) {
	std::printf ("gcc %s\n", __VERSION__);
	std::string test("gcc5/libstc++11");
	check (test);
	return 0;
}
