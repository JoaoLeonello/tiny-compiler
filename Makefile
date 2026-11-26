CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CFLAGS ?= -Wall -Wextra -O2

SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:src/%.cpp=build/%.o)
RUNTIME := build/runtime.o

all: minicc $(RUNTIME)

minicc: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/main.o: CPPFLAGS += -DRUNTIME_OBJ='"$(abspath $(RUNTIME))"'

build/%.o: src/%.cpp src/compiler.h | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(RUNTIME): runtime/runtime.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

test: all
	./tests/run.sh

clean:
	rm -rf build minicc

.PHONY: all test clean
