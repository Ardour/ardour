# Some basic utilities for testing the tranzport's I/O
# eventually "tranzport" will become a flexible command
#
#
all: tranzport tranzport_lights

tranzport: tranzport.c
	gcc -g -Wall -o tranzport tranzport.c 

tranzport_lights: tranzport_lights.c
	gcc -g -Wall -o tranzport_lights tranzport_lights.c 

clean:: 
	rm -f core .*.cmd *.o *.ko *.mod.c Module.symvers *.bak .\#* *~
	rm -rf .tmp_versions tranzport tranzport_lights


