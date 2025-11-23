SOURCES := $(wildcard src/*.cpp)
FLAGS=-Wall

build: $(SOURCES)
	g++ $(FLAGS) -O3 main.cpp $(SOURCES)

build-test: $(SOURCES)
	g++ $(FLAGS) -O0 -ggdb3 test.cpp $(SOURCES) -o run_test

test: build-test
	./run_test