CFLAGS= -Wall -std=gnu99 -g `pkg-config --cflags clutter-1.0` `pkg-config --cflags gtk+-2.0` 
LDFLAGS= `pkg-config --libs clutter-1.0` -lm `pkg-config --libs gtk+-2.0`
CC=gcc

all: texture

clean:
	rm *.o texture


texture: texture.o types.o log.o
	${CC} -Wall $(LDFLAGS) texture.o types.o log.o -o texture

types.o: types.c types.h
	${CC} -c -Wall $(CFLAGS) types.c

log.o: log.c log.h
	${CC} -c -Wall $(CFLAGS) log.c
