CFLAGS = -Wall
LOADLIBES = -ldl

gcc-glibmm-abi-check: abicheck.c
	$(CC) $(CPPFLAGS) $(CFLAGS) \
		-o gcc-glibmm-abi-check \
		abicheck.c \
		$(LDFLAGS) $(LOADLIBES)

check: gcc-glibmm-abi-check
	./gcc-glibmm-abi-check

gcc-glibmm-abi-check.1: gcc-glibmm-abi-check
	help2man -N \
		-n 'glib gcc4/5 C++11 ABI compatibility test' \
		-o gcc-glibmm-abi-check.1 \
		./gcc-glibmm-abi-check

.PHONY: check
