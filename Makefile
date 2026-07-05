# =========================
# COMPILER
# =========================
CXX      := g++
CC       := gcc

# =========================
# FLAGS
# =========================
CXXFLAGS := -std=c++20 -g3 -Wall -Wextra -Iinclude
CFLAGS   := -Wall -Wextra -Iinclude 
LDFLAGS  := -lxxhash -lssl -lcassandra -lDotenv -ltcmalloc -pthread -lmaxminddb -luring

# =========================
# DIRECTORIES
# =========================
SRC_DIR  := src
INC_DIR  := include
BUILD    := build
TARGET   := EoDNS

# =========================
# FIND ALL SOURCES (RECURSIVE)
# =========================
CPP_SRCS := $(shell find $(SRC_DIR) -type f -name '*.cpp')
C_SRCS   := $(shell find $(SRC_DIR) -type f -name '*.c')

OBJS := $(CPP_SRCS:$(SRC_DIR)/%.cpp=$(BUILD)/%.o) \
        $(C_SRCS:$(SRC_DIR)/%.c=$(BUILD)/%.o)

# =========================
# DEFAULT RULE
# =========================
all: $(TARGET)

# =========================
# LINK
# =========================
$(TARGET): $(OBJS)
	@echo "Linking $@"
	@$(CXX) $^ -o $@ $(LDFLAGS)

# =========================
# C++ COMPILE
# =========================
$(BUILD)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "CXX $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# =========================
# C COMPILE
# =========================
$(BUILD)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# =========================
# CLEAN
# =========================
clean:
	@echo "Cleaning"
	@rm -rf $(BUILD) $(TARGET)

# =========================
# PHONY
# =========================
.PHONY: all clean