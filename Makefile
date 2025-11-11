# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g -Iinclude
LDFLAGS = -lncurses

# Directories
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

# Files
SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC))
BIN = $(BINDIR)/filemgr

# Targets
all: $(BIN)

build: $(BIN)

# Create directories if they don't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Link object files to create binary
$(BIN): $(OBJDIR) $(BINDIR) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)
	@echo "Build complete: $(BIN)"

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run the program
run: all
	$(BIN)

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)
	@echo "Cleaned build artifacts"

# Rebuild from scratch
rebuild: clean all

# Show project structure
info:
	@echo "Source files: $(SRC)"
	@echo "Object files: $(OBJ)"
	@echo "Binary: $(BIN)"

.PHONY: all build run clean rebuild info
