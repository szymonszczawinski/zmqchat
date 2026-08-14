# =============================================================
# C23 PROJECT MAKEFILE (ZMQ + GoogleTest + Protobuf Edition)
# ============================================================

# --- Compilers ---
CC   := gcc
CXX  := g++
# --- Directories ---
SRC_DIR   := src
TEST_DIR  := tests
PROTO_DIR := proto
GEN_DIR   := src/generated

# --- Output Names ---
TEST_BIN := run_tests
# --- Mode: debug or release (dbg, rel) ---
MODE ?= dbg
# --- Target Type Selection (app, static, shared) ---
TYPE ?= app

ifeq ($(MODE), rel)
    CFLAGS_MODE := -O3 -DNDEBUG
    CXXFLAGS_MODE := -O3 -DNDEBUG
    OBJ_DIR := obj/release
else
    CFLAGS_MODE := -g -Og -DDEBUG
    CXXFLAGS_MODE := -g -Og -DDEBUG
    OBJ_DIR := obj/debug
endif

# --- OS Detection ---
ifeq ($(OS),Windows_NT)
    PLATFORM_OS := WINDOWS
    RAYLIB_PATH    := C:/Users/SSzczawinski/scoop/apps/mingw/15.2.0-rt_v13-rev0/mingw64
    RAYLIB_CFLAGS  := -I$(RAYLIB_PATH)/include
    RAYLIB_LIBS    := -L$(RAYLIB_PATH)/lib -lraylib -lopengl32 -lgdi32 -lwinmm
    BIN_EXT := .exe
else
    PLATFORM_OS := LINUX
    RAYLIB_PATH    := /usr
    RAYLIB_CFLAGS  := -I$(RAYLIB_PATH)/include
    RAYLIB_LIBS    := -L$(RAYLIB_PATH)/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    BIN_EXT :=
endif

# --- Output Names & Flags based on Type ---
ifeq ($(TYPE), static)
    TARGET_CFLAGS := -fPIC
    TARGET_CXXFLAGS := -fPIC
    ifeq ($(MODE), rel)
        BIN_NAME := libmy_project.a
    else
        BIN_NAME := libmy_project_debug.a
    endif
else ifeq ($(TYPE), shared)
    TARGET_CFLAGS := -fPIC
    TARGET_CXXFLAGS := -fPIC
    ifeq ($(PLATFORM_OS), WINDOWS)
        ifeq ($(MODE), rel)
            BIN_NAME := my_project.dll
        else
            BIN_NAME := my_project_debug.dll
        endif
    else
        ifeq ($(MODE), rel)
            BIN_NAME := libmy_project.so
        else
            BIN_NAME := libmy_project_debug.so
        endif
    endif
else
    TARGET_CFLAGS :=
    TARGET_CXXFLAGS :=
    ifeq ($(MODE), rel)
        BIN_NAME := my_project$(BIN_EXT)
    else
        BIN_NAME := my_project_debug$(BIN_EXT)
    endif
endif

# --- Libraries ---
ZMQ_CFLAGS := $(shell pkg-config --cflags libzmq)
ZMQ_LIBS   := $(shell pkg-config --libs libzmq)
PROTO_LIBS := -lprotobuf-c
GTEST_LIBS := -lgtest -lgtest_main -lpthread
LIBS       := $(ZMQ_LIBS) $(RAYLIB_LIBS) $(PROTO_LIBS) -lstdc++

# --- POPRAWKA 1: Dodano -I$(GEN_DIR) do flag kompilacji ---
INCLUDES := -Isrc -I$(GEN_DIR) $(ZMQ_CFLAGS) $(RAYLIB_CFLAGS)

# --- Global Flags ---
CFLAGS   := -std=c23 -Wall -Wextra $(INCLUDES) -MMD -MP  $(CFLAGS_MODE) $(TARGET_CFLAGS)
CXXFLAGS := -std=c++23 -Wall -Wextra $(INCLUDES) -MMD -MP $(CXXFLAGS_MODE) $(TARGET_CXXFLAGS)
LDFLAGS  := $(LIBS)

# =============================================================
# File Discovery & Protobuf Rules (POPRAWIONE)
# =============================================================

# 1. Jawna lista wygenerowanych plików systemowych protobuf-c
SYSTEM_PROTO_SRCS := $(GEN_DIR)/google/protobuf/any.pb-c.c \
                     $(GEN_DIR)/google/protobuf/wrappers.pb-c.c

# 2. Własne pliki proto
PROTOS     := $(shell find $(PROTO_DIR) -name "*.proto" 2>/dev/null)
PROTO_SRCS := $(patsubst $(PROTO_DIR)/%.proto,$(GEN_DIR)/%.pb-c.c,$(PROTOS))

# 3. Szukamy TYLKO plików źródłowych pisanych ręcznie w src/ (wykluczając src/generated)
HANDWRITTEN_SRCS := $(shell find $(SRC_DIR) -path "$(GEN_DIR)" -prune -o -name "*.c" -print 2>/dev/null)

# 4. Łączymy ręczne pliki C z wygenerowanymi (zamiast ponownego 'find' po src/)
ALL_SRCS := $(HANDWRITTEN_SRCS) $(PROTO_SRCS) $(SYSTEM_PROTO_SRCS)
SRCS     := $(sort $(ALL_SRCS))

# Mapowanie wszystkich src/*.c na obj/*.o
OBJS      := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

TEST_SRCS := $(shell find $(TEST_DIR) -name "*.cpp" 2>/dev/null)
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))

DEPS      := $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)
# =============================================================
# GOALS
# =============================================================

.PHONY: all clean run test full proto

all: proto $(BIN_NAME)

run: proto $(BIN_NAME)
	@echo "Running Application..."
	@./$(BIN_NAME) $(ARGS)
	
test: proto $(TEST_BIN)
	@echo "--- Running GoogleTests ---"
	@./$(TEST_BIN)

proto: system_proto $(PROTO_SRCS)

# --- POPRAWKA 3: Dedykowana reguła dla plików systemowych Any i Wrappers ---
system_proto: $(SYSTEM_PROTO_SRCS)

#==================================================================
# Protobuf Generation Rules
#==================================================================
# Wymuszamy, aby wygenerowanie plików .c zawsze poprawnie budowało cele .o
$(GEN_DIR)/google/protobuf/any.pb-c.c $(GEN_DIR)/google/protobuf/wrappers.pb-c.c:
	@mkdir -p $(GEN_DIR)
	@echo "Generating System Protobuf files (Any, Wrappers)..."
	@protoc --c_out=$(GEN_DIR) -I/usr/include /usr/include/google/protobuf/wrappers.proto /usr/include/google/protobuf/any.proto

$(PROTO_SRCS): $(GEN_DIR)/%.pb-c.c: $(PROTO_DIR)/%.proto
	@mkdir -p $(dir $@)
	@echo "Generating C Protobuf files for: $<"
	@protoc --c_out=$(GEN_DIR) --proto_path=$(PROTO_DIR) $<

# Informujemy Make'a, że przed kompilacją obiektów z GEN_DIR należy najpierw stworzyć pliki C
$(OBJ_DIR)/generated/%.o: $(GEN_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling generated C: $<"
	@$(CC) $(CFLAGS) -c $< -o $@
#==================================================================
# Linking Rules
#==================================================================

$(BIN_NAME): $(OBJS)
ifeq ($(TYPE), static)
	@echo "Creating Static Library: $@"
	@ar rcs $@ $(OBJS)
else ifeq ($(TYPE), shared)
	@echo "Linking Shared Library: $@"
	@$(CC) -shared $(OBJS) -o $@ $(LDFLAGS)
else
	@echo "Linking Application: $@"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)
endif

$(TEST_BIN): $(filter-out $(OBJ_DIR)/main.o, $(OBJS)) $(TEST_OBJS)
	@echo "Linking Tests: $@"
	@$(CXX) $^ -o $@ $(LDFLAGS) $(GTEST_LIBS)

#==================================================================
# Compilation Rules
#==================================================================

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling Test: $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@echo "Cleaning up..."
	@echo "Cleaning up obj..."
	@rm -rf obj
	@echo "Cleaning up generated files..."
	@rm -rf $(GEN_DIR)
	@echo "Cleaning up app binary..."
	@rm -rf $(BIN_NAME)
	@echo "Cleaning up test binary..."
	@rm -rf $(TEST_BIN)
	
full: clean all

-include $(DEPS)
