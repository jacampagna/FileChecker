.PHONY: all
all : xcheck

xcheck : xcheck.c
	gcc xcheck.c -std=c99 -Wall -Werror -O -o xcheck

.PHONY: clean
clean :
	rm -rf xcheck *.o *.dSYM

.PHONY: test
test :
	/u/c/s/cs537-1/tests/p5/runtests -c
	rm -f *.log