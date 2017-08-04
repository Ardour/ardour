all: bb

CXXFLAGS=-I../../libs/evoral `pkg-config --cflags jack` `pkg-config --cflags gtkmm-2.4`
LDFLAGS=`pkg-config --libs jack` `pkg-config --libs gtkmm-2.4`

bb: bb.o gui.o
	$(CXX) -o bb bb.o gui.o $(LDFLAGS) 


bb.o: bb.cc bb.h gui.h
gui.o: gui.cc gui.h bb.h

clean:
	rm -f bb bb.o gui.o
