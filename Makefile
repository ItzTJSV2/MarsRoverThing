# Config
CC = gcc
CFLAGS = -g -Wall -Wextra -fPIC
TARGET = runme
LIBTARGET = liballocator.so
OBJDIR = obj

# Source files
SRC = allocator.c runme.c
ALLOCATOR_SRC = allocator.c

# Object files
ALLOCATOR_OBJ = $(OBJDIR)/allocator.o
RUNME_OBJ = $(OBJDIR)/runme.o

# Default target
all: $(TARGET) $(LIBTARGET)

# Create obj directory if missing
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile allocator.c to PIC object for shared library
$(ALLOCATOR_OBJ): allocator.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c allocator.c -o $(ALLOCATOR_OBJ)

# Compile runme.c object
$(RUNME_OBJ): runme.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c runme.c -o $(RUNME_OBJ)

# Link executable runme
$(TARGET): $(ALLOCATOR_OBJ) $(RUNME_OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(ALLOCATOR_OBJ) $(RUNME_OBJ)

# Build shared library
$(LIBTARGET): $(ALLOCATOR_OBJ)
	$(CC) -shared -o $(LIBTARGET) $(ALLOCATOR_OBJ)

# Clean
clean:
	rm -rf $(OBJDIR) $(TARGET) $(LIBTARGET)

test:
	./runme

.PHONY: all clean
