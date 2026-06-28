BUILD_DIR = build
CXX = g++
CXXFLAGS = -std=c++17 -Wall

UTILS_PATH = commons/src/impl/utils/utils.cpp

SERVER_BIN = server_app

SERVER_SRCS = database/src/main.cpp \
							$(wildcard database/src/impl/*/*.cpp) \
							$(UTILS_PATH)

SERVER_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SERVER_SRCS))

CLIENT_BIN = client_app

CLIENT_SRCS = client/src/main.cpp \
							$(wildcard client/src/impl/*.cpp) \
							$(UTILS_PATH)

CLIENT_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(CLIENT_SRCS))

TEST_BIN = database/tests/run_tests.sh

TEST_SRCS = database/tests/b-tree.cpp database/tests/utils/tests_main.cpp

TEST_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(TEST_SRCS)) \
						$(filter-out build/database/src/main.o build/database/src/impl/server/server.o, $(SERVER_OBJS))

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN) : $(SERVER_OBJS)
	@echo "Linking Server..."
	$(CXX) $(CXXFLAGS) $^ -o $@

$(CLIENT_BIN) : $(CLIENT_OBJS)
	@echo "Linking Client..."
	$(CXX) $(CXXFLAGS) $^ -o $@

generate_data:
	@echo "Generating data for testing..." && \
	cd database/tests/data && \
	python data_generation.py && \
	python data_generation_small.py && \
	python data_generation_jumbled.py

test: $(TEST_BIN)
	@echo "Running tests..."
	@cd database/tests && ./run_tests.sh

$(TEST_BIN): $(TEST_OBJS)
	@echo "Linking Tests..."
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: %.cpp
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@echo "Nuking build directory..."
	rm -rf $(BUILD_DIR) $(SERVER_BIN) $(CLIENT_BIN)

.PHONY: all clean
