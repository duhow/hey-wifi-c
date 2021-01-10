LIBS = -lasound -lquiet -ljansson -lliquid -lfec
CFLAGS ?= -O3 -DLOG_USE_COLOR=1
CC = gcc


hey-wifi: main.c log.c
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	@rm -vf hey-wifi
