# Host build of the translated Kingdom (no 3DS needed).
CC ?= gcc
CFLAGS ?= -Ivendor -DLUME_HOST_JOBS -std=gnu11 -D_DEFAULT_SOURCE -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation -Wno-override-init
SRC = $(wildcard flash/*.c) $(wildcard flixel/*.c) $(wildcard game/*.c)

all: kingdom_host

kingdom_host: $(SRC) platform/host/main.c platform/host/media.c platform/common/session.c platform/common/stereo.c platform/common/parallel.c vendor/engine/core/jobs_state.c
	$(CC) $(CFLAGS) -o $@ $^ -lm -lpng -lpthread

clean:
	rm -f kingdom_host
