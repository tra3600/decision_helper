CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LIB = DecisionHelper.cpp Report.cpp Storage.cpp

all: decision_helper

decision_helper: main.cpp $(LIB)
	$(CXX) $(CXXFLAGS) -o $@ main.cpp $(LIB)

tests: tests.cpp $(LIB)
	$(CXX) $(CXXFLAGS) -o $@ tests.cpp $(LIB)

test: tests
	./tests

demo: decision_helper
	./decision_helper --demo

clean:
	rm -f decision_helper tests

.PHONY: all test demo clean
