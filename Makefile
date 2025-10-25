# Make help the default output so users discover the available tooling targets.
.DEFAULT_GOAL := help

# ------------------------------------------------------------------------------
# Tooling configuration
# ------------------------------------------------------------------------------
CXX       ?= g++
CXXFLAGS  ?= -std=c++17 -Wall -Wextra
SRC_DIR   := Firmware/motherBoard/src
TEST_DIR  := tests
BUILD_DIR := build
HOST_DEFINES := -DINCUNEST_HOST_BUILD
INCLUDES  := -I./$(SRC_DIR) -I./Firmware/motherBoard/include -I./Firmware/motherBoard/include/host_stubs

LINT_TOOL   ?= cpplint
FORMAT_TOOL ?= clang-format
LDLIBS      ?= -lgtest -lgtest_main -pthread

# ------------------------------------------------------------------------------
# Source discovery
# ------------------------------------------------------------------------------
SRCS        := $(wildcard $(SRC_DIR)/*.cpp)
HDRS        := $(wildcard $(SRC_DIR)/*.h)
OBJS        := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Split out main.cpp so test builds can reuse the rest of the sources
NON_MAIN_SRCS := $(filter-out $(SRC_DIR)/main.cpp,$(SRCS))
TEST_SRCS     := $(wildcard $(TEST_DIR)/test_*.cpp)

# ------------------------------------------------------------------------------
# Default target – host build to catch compile issues early
# ------------------------------------------------------------------------------
all: firmware.out

firmware.out: $(OBJS)
	$(CXX) $(CXXFLAGS) $(HOST_DEFINES) $(INCLUDES) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(HOST_DEFINES) $(INCLUDES) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $@

# ------------------------------------------------------------------------------
# Lint – Google C++ style via cpplint
# ------------------------------------------------------------------------------
lint:
	$(LINT_TOOL) $(SRCS) $(HDRS)

# ------------------------------------------------------------------------------
# Format – enforce Google style via clang-format
# ------------------------------------------------------------------------------
format:
	$(FORMAT_TOOL) -style=google -i $(SRCS) $(HDRS)

# ------------------------------------------------------------------------------
# Tests – build everything except main.cpp + link GoogleTest suites
# ------------------------------------------------------------------------------
ifeq ($(strip $(TEST_SRCS)),)
test:
	@echo "No tests found in $(TEST_DIR); nothing to run."
else
test: run_tests
	./run_tests

run_tests: $(NON_MAIN_SRCS) $(TEST_SRCS)
	$(CXX) $(CXXFLAGS) $(HOST_DEFINES) $(INCLUDES) $^ -o $@ $(LDLIBS)
endif

# ------------------------------------------------------------------------------
# Clean – remove build artifacts
# ------------------------------------------------------------------------------
clean:
	rm -rf $(BUILD_DIR) firmware.out run_tests


# ------------------------------------------------------------------------------
# Deps – install cpplint, clang-format, and gtest toolchain prerequisites
# ------------------------------------------------------------------------------
deps:
	@set -e; \
	OS=$$(uname -s); \
	if [ "$$OS" = "Darwin" ]; then \
		if command -v brew >/dev/null 2>&1; then \
			brew update; \
			brew install cpplint clang-format googletest || true; \
		else \
			echo "Homebrew not found. Install it from https://brew.sh to continue." >&2; \
			exit 1; \
		fi; \
	elif [ "$$OS" = "Linux" ]; then \
		if command -v apt-get >/dev/null 2>&1; then \
			sudo apt-get update; \
			sudo apt-get install -y cpplint clang-format build-essential cmake libgtest-dev || exit 1; \
			if [ -d /usr/src/googletest ]; then \
				cd /usr/src/googletest && sudo cmake . && sudo cmake --build . --target install; \
			fi; \
		else \
			echo "Unsupported Linux package manager. Install clang-format, googletest, and pip manually." >&2; \
			exit 1; \
		fi; \
	else \
		echo "Unsupported OS: $$OS. Install clang-format, googletest, and cpplint manually." >&2; \
		exit 1; \
	fi; \

# ------------------------------------------------------------------------------
# Help – list the most useful targets
# ------------------------------------------------------------------------------
help:
	@echo "Available targets:"
	@echo "  make all      - Build firmware.out from all sources for host testing"
	@echo "  make lint     - Run cpplint (Google C++ style) on active firmware sources"
	@echo "  make format   - Apply clang-format -style=google to active firmware sources"
	@echo "  make test     - Build & execute GoogleTest suites in ./tests"
	@echo "  make clean    - Remove build artifacts (build/, firmware.out, run_tests)"
	@echo "  make deps     - Install cpplint, clang-format, and gtest toolchain prerequisites"

.PHONY: help all lint format test clean deps
