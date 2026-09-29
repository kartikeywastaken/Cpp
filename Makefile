CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -Iinclude

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
BIN_DIR = bin

SRCS = $(filter-out $(SRC_DIR)/main.cpp, $(wildcard $(SRC_DIR)/*.cpp))
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

COMPILER_MAIN = $(SRC_DIR)/main.cpp
COMPILER_BIN = $(BIN_DIR)/c_compiler

TEST_SRCS = $(wildcard $(TEST_DIR)/*.cpp)
TEST_BIN = $(BIN_DIR)/run_tests

.PHONY: all compiler test clean

all: compiler test

compiler: $(COMPILER_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(COMPILER_BIN): $(OBJS) $(COMPILER_MAIN) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) $(COMPILER_MAIN) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(OBJS) $(TEST_SRCS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) $(TEST_SRCS) -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) *.o
