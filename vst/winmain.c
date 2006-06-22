#include <limits.h>
#include <unistd.h>
#include <stdio.h>

extern int ardour_main(int argc, char* argv[]);

int
main (int argc, char* argv[]) 
{
    // call the user specified main function    
    
	int result = ardour_main(argc, argv);
	printf ("main returned %d\n", result);
	
	return result;

}
