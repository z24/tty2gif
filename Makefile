CXX     = g++
CFLAGS	= -Wall -g
LDFLAGS	=
LIBS    =
TARGETS	= tty2gif

tty2gif: tty2gif.cpp
	${CXX} ${CFLAGS} ${LDFLAGS} -o $@ $^ ${LIBS}

clean:
	rm -f ${TARGETS}
