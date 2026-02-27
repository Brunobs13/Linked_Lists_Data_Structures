CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC
PG_INCLUDE_DIR := $(shell pg_config --includedir 2>/dev/null)
PG_LIB_DIR := $(shell pg_config --libdir 2>/dev/null)
INCLUDES := -Iinclude $(if $(PG_INCLUDE_DIR),-I$(PG_INCLUDE_DIR))
comma := ,
RPATH_FLAGS := $(if $(PG_LIB_DIR),-Wl$(comma)-rpath$(comma)$(PG_LIB_DIR))
OBJ_DIR := build/obj
BUILD_DIR := build
BUILD_STAMP := $(BUILD_DIR)/.dir

ENGINE_LIB := $(BUILD_DIR)/liblog_engine.so
ENGINE_BIN := $(BUILD_DIR)/log_engine

CORE_SRCS := \
	src/core/log_entry.c \
	src/core/linked_list.c \
	src/core/buffer_engine.c \
	src/core/queue_processor.c

DB_SRCS := src/db/persistence.c
UTIL_SRCS := src/utils/logger.c src/utils/config.c
API_SRCS := src/api/engine_api.c
MAIN_SRCS := src/main.c

ENGINE_SRCS := $(CORE_SRCS) $(DB_SRCS) $(UTIL_SRCS) $(API_SRCS)
ENGINE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(ENGINE_SRCS))
MAIN_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(MAIN_SRCS))

LIBS := $(if $(PG_LIB_DIR),-L$(PG_LIB_DIR)) $(RPATH_FLAGS) -lpq -lpthread

TEST_LINKED_LIST := $(BUILD_DIR)/test_linked_list
TEST_BUFFER_ENGINE := $(BUILD_DIR)/test_buffer_engine

.PHONY: all build build-lib build-bin run-api run-engine test clean docker-up docker-down

all: build

build: build-lib build-bin

build-lib: $(ENGINE_LIB)

build-bin: $(ENGINE_BIN)

$(ENGINE_LIB): $(ENGINE_OBJS) | $(BUILD_STAMP)
	$(CC) -shared -o $@ $(ENGINE_OBJS) $(LIBS)

$(ENGINE_BIN): $(MAIN_OBJS) $(ENGINE_OBJS) | $(BUILD_STAMP)
	$(CC) -o $@ $(MAIN_OBJS) $(ENGINE_OBJS) $(LIBS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_STAMP):
	mkdir -p $(BUILD_DIR)
	touch $(BUILD_STAMP)

$(TEST_LINKED_LIST): tests/test_linked_list.c src/core/log_entry.c src/core/linked_list.c | $(BUILD_STAMP)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ -lpthread

$(TEST_BUFFER_ENGINE): tests/test_buffer_engine.c src/core/log_entry.c src/core/linked_list.c src/core/buffer_engine.c src/utils/logger.c | $(BUILD_STAMP)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ -lpthread

run-engine: $(ENGINE_BIN)
	./$(ENGINE_BIN)

run-api: $(ENGINE_LIB)
	ENGINE_LIB_PATH=$(ENGINE_LIB) uvicorn src.api.app:app --host 0.0.0.0 --port $${API_PORT:-8000}

test: $(TEST_LINKED_LIST) $(TEST_BUFFER_ENGINE)
	./$(TEST_LINKED_LIST)
	./$(TEST_BUFFER_ENGINE)

clean:
	rm -rf $(BUILD_DIR)

docker-up:
	docker-compose up --build

docker-down:
	docker-compose down -v
