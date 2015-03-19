TARGET=mle
CC=colorgcc
SHELL=/bin/bash

all: mle

mle: mlbuf.a ./termbox/build/src/libtermbox.a
	$(CC) -Wall -pg -g -I./mlbuf/ -L./mlbuf/ *.c -o $@ ./mlbuf/libmlbuf.a ./termbox/build/src/libtermbox.a -lpcre -lm

mlbuf.a:
	git submodule update --recursive
	make -C mlbuf

./termbox/build/src/libtermbox.a:
	pushd termbox; ./waf configure && ./waf; popd

clean:
	rm -f *.o
	rm -f gmon.out
	rm -f mle
	make -C mlbuf clean
	pushd termbox; ./waf clean; popd
