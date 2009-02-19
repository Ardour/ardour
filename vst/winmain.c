#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <fst.h>

extern int ardour_main(int argc, char* argv[]);

int
main (int argc, char* argv[]) 
{
  return ardour_main(argc, argv);
}
