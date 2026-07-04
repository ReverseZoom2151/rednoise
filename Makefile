PROJECT_NAME := RedNoise
BUILD_DIR    := build
EXECUTABLE   := $(BUILD_DIR)/$(PROJECT_NAME)

# All translation units: the app and the framework (glm 1.0.1 is header-only).
SOURCE_FILES := src/RedNoise.cpp $(wildcard framework/*.cpp)

# Build settings
COMPILER         := clang++
COMPILER_OPTIONS := -pipe -std=c++17
INCLUDES         := -Iframework -Ithird_party
WARNINGS         := -Wall
DEBUG_OPTIONS    := -ggdb -g3
FUSSY_OPTIONS    := -Werror -pedantic
SANITIZER_OPTS   := -O1 -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer
SPEEDY_OPTIONS   := -Ofast -funsafe-math-optimizations -march=native

# SDL2 flags (falls back gracefully if sdl2-config is on PATH)
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

CXXFLAGS := $(COMPILER_OPTIONS) $(WARNINGS) $(INCLUDES) $(SDL_CFLAGS)

default: debug

# For use with a debugger (works fine without one too).
debug:
	@mkdir -p $(BUILD_DIR)
	$(COMPILER) $(CXXFLAGS) $(DEBUG_OPTIONS) -o $(EXECUTABLE) $(SOURCE_FILES) $(SDL_LIBS)
	./$(EXECUTABLE)

# Helps track down segfaults / UB.
diagnostic:
	@mkdir -p $(BUILD_DIR)
	$(COMPILER) $(CXXFLAGS) $(FUSSY_OPTIONS) $(SANITIZER_OPTS) -o $(EXECUTABLE) $(SOURCE_FILES) $(SDL_LIBS)
	./$(EXECUTABLE)

# High-performance build for interactive testing.
speedy:
	@mkdir -p $(BUILD_DIR)
	$(COMPILER) $(CXXFLAGS) $(SPEEDY_OPTIONS) -o $(EXECUTABLE) $(SOURCE_FILES) $(SDL_LIBS)
	./$(EXECUTABLE)

# Final production release.
production:
	@mkdir -p $(BUILD_DIR)
	$(COMPILER) $(CXXFLAGS) -O2 -o $(EXECUTABLE) $(SOURCE_FILES) $(SDL_LIBS)
	./$(EXECUTABLE)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: default debug diagnostic speedy production clean
