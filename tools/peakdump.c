// gcc -o peakdump peakdump.c -Wall -O3 -lm
// inspect ardour .peak files

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define _FPP 256

int main (int argc, char **argv) {
	int c = 0, d = 0;
	float thresh = -1.f;

	if (argc < 2 || argc > 3) {
		fprintf(stderr, "usage: %s [threshold] <peakfile>\n", argv[0]);
		return -1;
	}

	if (argc == 3) {
		thresh = atof(argv[1]);
	}

	FILE *F = fopen(argv[argc-1], "r");

	if (!F) {
		fprintf(stderr, "Cannot open file '%s'\n", argv[argc-1]);
		return -1;
	}

	printf("   #    )   audio sample range   :   MIN    MAX\n");
	while (!feof(F)) {
		struct PeakData {
			float min;
			float max;
		} buf;

		if (fread(&buf, sizeof(struct PeakData), 1, F) <= 0) {
			break;
		}
		if (fabsf(buf.min) > thresh || fabsf(buf.max) > thresh) {
			printf("%8d) %10d - %10d: %+.3f %+.3f\n", ++d,
					_FPP * c, _FPP * (c + 1) - 1,
					buf.min, buf.max);
		}
		++c;
	}
	fclose(F);
	return 0;
}
