# Define the name of your pass
PASS_NAME = wdsymys

# Use platform-specific shared library extension
ifeq ($(shell uname -s), Darwin)
    LIBRARY_NAME = $(PASS_NAME).dylib
    SHARED_FLAG = -dynamiclib
else
    LIBRARY_NAME = $(PASS_NAME).so
    SHARED_FLAG = -shared
endif

# Use llvm-config to get compiler and linker flags
LLVM_CONFIG = llvm-config
LLVM_CXXFLAGS = $(shell $(LLVM_CONFIG) --cxxflags) -fno-rtti -fPIC
LLVM_LDFLAGS = $(shell $(LLVM_CONFIG) --ldflags --system-libs --libs core irreader passes support)

# Compiler and linker
CXX = clang++
CLANG = clang
CXXFLAGS = -std=c++17 -Wall -Wextra $(LLVM_CXXFLAGS) -g3 -O0
LDFLAGS = $(LLVM_LDFLAGS)

# Directories
BUILD_DIR = .build
SRC_DIR = wdsymys
TESTCASES_DIR = testcases
TESTCASES_BUILD_DIR = $(BUILD_DIR)/$(TESTCASES_DIR)

# Source and object files
SRCS = $(SRC_DIR)/pass.cpp $(SRC_DIR)/RegisterPackerContext.cpp
OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Test cases
TESTCASES = $(wildcard $(TESTCASES_DIR)/*.cpp)
TESTCASE_BC_FILES = $(patsubst $(TESTCASES_DIR)/%.cpp,$(TESTCASES_BUILD_DIR)/%.bc,$(TESTCASES))

_default: all
.PHONY: _default

# Ensure build directories exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TESTCASES_BUILD_DIR):
	mkdir -p $(TESTCASES_BUILD_DIR)

# Build rules
all: $(BUILD_DIR) $(TESTCASES_BUILD_DIR) $(BUILD_DIR)/$(LIBRARY_NAME) $(TESTCASE_BC_FILES)
.PHONY: all

$(BUILD_DIR)/$(LIBRARY_NAME): $(OBJS)
	$(CXX) $(SHARED_FLAG) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TESTCASES_BUILD_DIR)/%.bc: $(TESTCASES_DIR)/%.cpp | $(TESTCASES_BUILD_DIR)
	$(CLANG) -c -Xclang -disable-O0-optnone -emit-llvm $< -o $@

# Clean up generated files
clean:
	rm -rf $(BUILD_DIR)
