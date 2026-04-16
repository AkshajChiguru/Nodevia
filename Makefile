# ══════════════════════════════════════════════════════════════════
#  Nodevia V1 — Makefile (Modular Structure Fixed)
# ══════════════════════════════════════════════════════════════════

CXX           := g++
CXXFLAGS      := -std=c++17 -Wall -Wextra -Wpedantic -Iinclude
RELEASE_FLAGS := -O2 -DNDEBUG
DEBUG_FLAGS   := -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS       := -lpthread

SRC_DIR  := src
OBJ_DIR  := build/obj
BIN_DIR  := build

TARGET       := $(BIN_DIR)/nodevia
TARGET_DEBUG := $(BIN_DIR)/nodevia_debug

# ✅ UPDATED FOR NEW FOLDER STRUCTURE
SRCS := \
$(SRC_DIR)/main.cpp \
$(SRC_DIR)/core/block.cpp \
$(SRC_DIR)/core/blockchain.cpp \
$(SRC_DIR)/core/transaction.cpp \
$(SRC_DIR)/crypto/pow.cpp \
$(SRC_DIR)/utils/utils.cpp \
$(SRC_DIR)/utils/serializer.cpp \
$(SRC_DIR)/wallet/wallet.cpp \
$(SRC_DIR)/network/seed.cpp \
$(SRC_DIR)/network/message.cpp \
$(SRC_DIR)/network/network.cpp

# Object files (auto-generated paths)
OBJS       := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o,       $(SRCS))
OBJS_DEBUG := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%_debug.o, $(SRCS))

.PHONY: all
all: $(TARGET)
	@echo ""
	@echo "  Build complete -> $(TARGET)"
	@echo ""

# 🔧 Release build
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $^ -o $@ $(LDFLAGS)

# 🔧 Debug build
$(TARGET_DEBUG): $(OBJS_DEBUG) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $^ -o $@ $(LDFLAGS)

# ✅ FIXED RULE (supports folders like core/, network/, etc.)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "  CC  $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%_debug.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "  CC  [debug] $<"
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -MMD -MP -c $< -o $@

# Auto dependency include
-include $(OBJS:.o=.d)
-include $(OBJS_DEBUG:.o=.d)

# Create directories
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

.PHONY: debug
debug: $(TARGET_DEBUG)

.PHONY: run
run: all
	@$(TARGET)

.PHONY: rebuild
rebuild: clean all

.PHONY: check
check:
	@for f in $(SRCS); do \
		[ -f $$f ] && echo "  OK  $$f" || echo "  MISSING: $$f"; done

.PHONY: clean
clean:
	@rm -rf $(OBJ_DIR) $(BIN_DIR) *.dat
	@echo "  Cleaned."