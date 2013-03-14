CFLAGS= -Wall -std=gnu99 -g `pkg-config --cflags clutter-1.0` `pkg-config --cflags gtk+-2.0` 
LDFLAGS= `pkg-config --libs clutter-1.0` -lm `pkg-config --libs gtk+-2.0 gthread-2.0`

CC=gcc

all: texture

clean:
	rm *.o texture


texture: texture.o types.o log.o
	${CC} -o texture -Wall  texture.o types.o log.o $(LDFLAGS) 

types.o: types.c types.h
	${CC} types.c -c -Wall $(CFLAGS)

texture.o: texture.c types.h log.h
	${CC} texture.c -c -Wall $(CFLAGS)

log.o: log.c log.h
	${CC} log.c -c -Wall $(CFLAGS)
