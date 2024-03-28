# Copyright (c) 2024 Renesas Electronics Corp.
# SPDX-License-Identifier: MIT-0

# Add compile flags
CFLAGS = -Wall -Wextra -Werror

# Add linking flags
LDFLAGS = -lm -lomxr_core -lpthread

# Get common source files
SRCS = omx.c main.c

# Get common object files
OBJS = $(SRCS:%.c=%.o)

# Define sample apps
APP = encoder

# Make sure 'all' and 'clean' are not files
.PHONY: all clean

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f  $(APP)
	rm -f  *.o
