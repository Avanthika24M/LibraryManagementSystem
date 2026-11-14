CXX ?= g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -O2 -Iinclude
SOURCES := src/main.cpp src/library.cpp src/security.cpp src/storage.cpp
TARGET := build/library_app

.PHONY: all run clean
all: $(TARGET)

$(TARGET): $(SOURCES) include/library.hpp
	@mkdir -p build data
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -rf build
