CXX     = g++
CFLAGS	= -O2 $(shell pkg-config --cflags Magick++)
LIBS    = $(shell pkg-config --libs Magick++) -lX11
TARGETS	= tty2gif

UNAME	= $(shell uname -s)
ifeq ($(UNAME), Darwin)
	CXX     = clang++
	CFLAGS += -I/opt/X11/include
endif

tty2gif: tty2gif.cpp
	${CXX} ${CFLAGS} -o $@ $^ ${LIBS}

clean:
	rm -f ${TARGETS}
