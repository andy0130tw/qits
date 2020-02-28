CXX ?= g++
CPPFLAGS = -g -Wall -Wno-unused-result

all: qits
clean:
	rm -f qits
zobrist_values:
	python scripts/gen_zobrist_values.py
