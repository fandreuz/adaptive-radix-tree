SOURCES := $(wildcard src/*.cpp)
ALL_SOURCES := $(wildcard src/*.cpp) $(wildcard src/*.hpp) ./*.cpp
FLAGS=-Wall

format:
	clang-format-19 --sort-includes -i $(ALL_SOURCES)

build: $(SOURCES)
	g++ $(FLAGS) -O3 main.cpp $(SOURCES)

build-test: $(SOURCES)
	g++ $(FLAGS) -O0 -ggdb3 test.cpp $(SOURCES) -o run_test

test: build-test
	./run_test