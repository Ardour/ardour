TARGET=doxy2json

CXX=clang++
CXXFLAGS=`llvm-config --cxxflags --ldflags` -lclang -std=c++11 -O2 -Wall -Wextra -pthread
LOADLIBES=-lclang

ifeq ($(shell uname -s), Darwin)
CXXFLAGS+=-stdlib=libstdc++
endif

all: $(TARGET)

$(TARGET): $(TARGET).cc

clean:
	@rm -f $(TARGET)

.PHONY: test
