#compiler selection
CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -g
LDFLAGS ?=

$(info Using compiler : $(CC))

#source and object files
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
TARGET = writer

#default target
all: $(TARGET)

#linking executables
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

#compile c -> o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

#clean target
clean:
	rm -f $(TARGET) $(OBJ)
