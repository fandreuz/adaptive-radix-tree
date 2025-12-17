SOURCES := $(wildcard src/*.cpp)
ALL_SOURCES := $(wildcard src/*.cpp) $(wildcard src/*.hpp) ./*.cpp
FLAGS=-std=c++11 -Wall -O3

build: $(SOURCES)
	g++ $(FLAGS) $(SOURCES)

format:
	clang-format-19 --sort-includes -i $(ALL_SOURCES)

build-test: $(SOURCES)
	g++ $(FLAGS) test.cpp $(SOURCES) -o run_test

test: build-test
	./run_test

build-bench: $(SOURCES)
	g++ $(FLAGS) bench.cpp $(SOURCES) -o run_bench

bench: build-bench
	./run_bench
