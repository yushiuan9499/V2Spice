CXX=g++
CFLAGS=-g

SRC_DIR=src
BUILD_DIR=build

HEADERS=$(SRC_DIR)/defs.h $(SRC_DIR)/lexer.h $(SRC_DIR)/parser.h $(SRC_DIR)/sema.h \
		$(SRC_DIR)/print.h

.PHONY: all clean
all: $(BUILD_DIR)/main

$(BUILD_DIR)/main: $(BUILD_DIR)/main.o $(BUILD_DIR)/string.o $(BUILD_DIR)/lexer.o $(BUILD_DIR)/parser.o $(BUILD_DIR)/sema.o $(BUILD_DIR)/print.o
	$(CXX) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp $(HEADERS)
	$(CXX) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: $(SRC_DIR)/string.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lexer.o: $(SRC_DIR)/lexer.cpp $(SRC_DIR)/lexer.h $(SRC_DIR)/defs.h
	$(CXX) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/parser.o: $(SRC_DIR)/parser.cpp $(SRC_DIR)/parser.h $(SRC_DIR)/defs.h
	$(CXX) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sema.o: $(SRC_DIR)/sema.cpp $(SRC_DIR)/sema.h $(SRC_DIR)/defs.h
	$(CXX) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/print.o: $(SRC_DIR)/print.cpp $(SRC_DIR)/print.h $(SRC_DIR)/defs.h
	$(CXX) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/main
