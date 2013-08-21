CXX     = g++
CFLAGS	= -O $(shell pkg-config --cflags Magick++)
LIBS    = $(shell pkg-config --libs Magick++)
TARGETS	= tty2gif

tty2gif: tty2gif.cpp
	${CXX} ${CFLAGS} -o $@ $^ ${LIBS}

clean:
	rm -f ${TARGETS}
