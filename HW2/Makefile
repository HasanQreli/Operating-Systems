# Compiler options
CC = g++
CFLAGS = -Wall -Wextra -std=c++11 -g
LDFLAGS = -pthread

# Source files
SRCS = main.cpp helper.c WriteOutput.c
OBJS = $(SRCS:.cpp=.o)
OBJS := $(OBJS:.c=.o)


# Executable name
TARGET = simulator

# Target for compiling all
all: $(TARGET)

# Target for linking the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# Rule for compiling C++ files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for compiling C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(OBJS) $(TARGET)
