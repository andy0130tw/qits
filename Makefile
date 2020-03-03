CXX ?= g++
CPPFLAGS = -O2 -Wall -Wno-unused-result
LDLIBS = board_view.o

all: board_view.o qits
clean:
	rm -f board_view.o qits
zobrist_values:
	python scripts/gen_zobrist_values.py > zobrist_values.h
