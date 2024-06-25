CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
SRC = main.c parser.c
OBJ = $(SRC:.c=.o)
EXEC = eshell

.PHONY: all clean

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(EXEC)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(EXEC) $(OBJ)
