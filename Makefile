LIBS = -lasound -lquiet -ljansson -lliquid -lfec
CFLAGS ?= -O3 -DLOG_USE_COLOR=1
CC = gcc

C_FILES = src/main.c src/log.c

hey-wifi: $(C_FILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	@rm -vf hey-wifi
