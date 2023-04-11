CC = g++
CFLAGS = -O2 ggdb -DDEBUG
CXXFLAGS+=-Wall -Wextra
LDFLAGS = -lpthread

all: sudoku_solve

sudoku_solve: main.cpp sudoku.cpp
	g++ -O2 -o $@ $^ -lpthread
	
.PHONY:clean
clean:
	rm -f sudoku_solve
