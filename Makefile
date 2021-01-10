LIBS = -lasound -lquiet -ljansson -lliquid -lfec
CFLAGS ?= -O3
CC = gcc


hey-wifi: main.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	@rm -vf hey-wifi
