#include <cstdlib>

#include <pbd/unescape.h>

void
unescape (char *str)

{
	char *p;
	bool escaped;
	long offset;
	char octal[4];
	int noct;
	char hex[3];
	int nhex;

	escaped = false;
	offset = 0;
	octal[3] = '\0';
	hex[2] = '\0';

	p = str;

	while (*p) {
		if (!escaped) {
			if (*p == '\\') {
				escaped = true;
			} else {
				*(p-offset) = *p;
			}
			p++;
			continue;
		}

		switch (*p) {
		case 'f':
			offset++;
			*(p-offset) = '\f';
			break;
		case 'r':
			offset++;
			*(p-offset) = '\r';
			break;
			
		case 'v':
			offset++;
			*(p-offset) = '\v';
			break;
			
		case 'n':
			offset++;
			*(p-offset) = '\n';
			break;
			
		case 't':
			offset++;
			*(p-offset) = '\t';
			break;
			
		case 'b':
			offset++;
			*(p-offset) = '\b';
			break;
			
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			noct = 0;
			while (noct < 3 &&
			       (*p >= '0' && *p <= '7')) {
				octal[noct++] = *p;
				offset++;
				p++;
			}
			p--;
			*(p-offset) = strtol (octal, 0, 8);
			break;
			
		case 'x':
			nhex = 0;
			p++;
			offset++;
			
			while (nhex < 2 &&
			       ((*p >= '0' && *p <= '9') ||
				(*p >= 'a' && *p <= 'f') ||
				(*p >= 'A' && *p <= 'F'))) {
				hex[nhex++] = *p;
				offset++;
				p++;
			}
			p--;
			*(p-offset) = strtol (hex, 0, 16);
			break;
			
		case '\\':
			offset++;
			*(p-offset) = '\\';
			break;
			
		case '"':
			offset++;
			*(p-offset) = '"';
			break;

		case '\'':
			offset++;
			*(p-offset) = '\'';
			break;

		default:
			*(p-offset) = *p;
		}

		escaped = false;
		p++;
	}
	
	*(p-offset) = '\0';
}

#ifdef TEST
#include <cstdio>

main (int argc, char *argv[])

{
	unescape (argv[1]);
	printf ("%s\n", argv[1]);
}

#endif
