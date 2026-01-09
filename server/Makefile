#compiler selection
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
LDFLAGS ?= -lpthread -lrt

$(info Using compiler : $(CC))

#source and object files
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
TARGET ?= aesdsocket

#default target
all: $(TARGET)

#linking executables
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

#compile c -> o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

#clean target
clean:
	rm -f $(TARGET) $(OBJ)
