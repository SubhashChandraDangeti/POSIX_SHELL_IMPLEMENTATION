CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -Wshadow -g
LDLIBS ?= -lreadline

TARGET := myshell
SOURCES := Main.cpp $(wildcard builtins/*.cpp system/*.cpp utils/*.cpp)
OBJECTS := $(patsubst %.cpp,build/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean test test-interactive
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/parser_test: tests/parser_test.cpp utils/parser.cpp utils/parser.h utils/helpers.cpp utils/helpers.h
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/parser_test.cpp utils/parser.cpp utils/helpers.cpp -o $@

build/integration_test: tests/integration_test.cpp
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -lutil -o $@

test: $(TARGET) build/parser_test build/integration_test
	./build/parser_test
	./build/integration_test ./$(TARGET)

test-interactive: $(TARGET) build/integration_test
	./build/integration_test ./$(TARGET) --interactive

clean:
	rm -rf build
	rm -f $(TARGET) Main.o builtins/*.o system/*.o utils/*.o

-include $(DEPS)
