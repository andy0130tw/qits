CXX ?= g++
CPPFLAGS = -O2 -flto -Wall -Wno-unused-result
LIBS = qits.o board_view.o

LINK.o = $(LINK.cc)

all: qits
qits: $(LIBS)

clean:
	rm -f qits.o board_view.o qits
zobrist_values:
	python scripts/gen_zobrist_values.py > zobrist_values.h

qits.o: qits.h
board_view.o: qits.h board_view.h
