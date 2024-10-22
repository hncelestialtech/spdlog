# Makefile for cel_spdlog

# Default target
all: install

# Install target
install:
	conan create .

# Clean target
clean:
	rm -rf build

# Phony targets
.PHONY: all install clean
