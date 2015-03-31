SHELL=/bin/bash
CC=$(shell if which colorgcc>/dev/null; then echo colorgcc; else echo gcc; fi)

all: mle

mle: *.c *.h ./mlbuf/libmlbuf.a ./termbox/build/src/libtermbox.a
	$(CC) -D_GNU_SOURCE -Wall -Wno-missing-braces -pg -g -I./mlbuf/ -I./termbox/src/ *.c -o $@ ./mlbuf/libmlbuf.a ./termbox/build/src/libtermbox.a -lpcre -lm

./mlbuf/libmlbuf.a:
	make -C mlbuf

./termbox/build/src/libtermbox.a:
	pushd termbox; ./waf configure && ./waf; popd

test: mle
	make -C mlbuf test
	./mle -v

clean:
	rm -f *.o
	rm -f gmon.out
	rm -f mle
	make -C mlbuf clean
	pushd termbox; ./waf clean; popd
