# Makefile for mini-spice
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -I./include
LDFLAGS = -lm

# Source directories
SRC_CORE = src/core
SRC_PARSER = src/parser
SRC_DEVICES = src/devices
SRC_ANALYSIS = src/analysis
SRC_MATH = src/math
SRC_OUTPUT = src/output

# Source files
CORE_SRCS = $(SRC_CORE)/circuit.c
PARSER_SRCS = $(SRC_PARSER)/parser.c
DEVICE_SRCS = $(SRC_DEVICES)/res.c \
              $(SRC_DEVICES)/cap.c \
              $(SRC_DEVICES)/ind.c \
              $(SRC_DEVICES)/vsrc.c \
              $(SRC_DEVICES)/isrc.c \
              $(SRC_DEVICES)/vccs.c \
              $(SRC_DEVICES)/vcvs.c \
              $(SRC_DEVICES)/cccs.c \
              $(SRC_DEVICES)/ccvs.c \
              $(SRC_DEVICES)/dio.c \
              $(SRC_DEVICES)/devreg.c
ANALYSIS_SRCS = $(SRC_ANALYSIS)/dcop.c \
                $(SRC_ANALYSIS)/dcsweep.c \
                $(SRC_ANALYSIS)/acan.c \
                $(SRC_ANALYSIS)/dctran.c \
                $(SRC_ANALYSIS)/anareg.c
MATH_SRCS = $(SRC_MATH)/sparse.c
OUTPUT_SRCS = $(SRC_OUTPUT)/output.c

MAIN_SRC = src/main.c

# Object files
OBJS = $(CORE_SRCS:.c=.o) \
       $(PARSER_SRCS:.c=.o) \
       $(DEVICE_SRCS:.c=.o) \
       $(ANALYSIS_SRCS:.c=.o) \
       $(MATH_SRCS:.c=.o) \
       $(OUTPUT_SRCS:.c=.o)

MAIN_OBJ = $(MAIN_SRC:.c=.o)

# Target
TARGET = mini-spice

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean
clean:
	rm -f $(OBJS) $(MAIN_OBJ) $(TARGET)
	rm -f src/*/*.o src/*.o

# Test
test: $(TARGET)
	@echo "Running test circuits..."
	./$(TARGET) tests/rc_filter.net
	./$(TARGET) tests/diode_clipper.net
	./$(TARGET) tests/rc_transient.net

# Install
install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/

.PHONY: all clean test install
