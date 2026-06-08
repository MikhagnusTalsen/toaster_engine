CXX = g++
CXX_FLAGS = -std=c++14 -Wall -O3 -march=native -flto=auto

TARGET = engine
SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXX_FLAGS) $(OBJ) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXX_FLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJ)

# g++ -std=c++14 -O3 -march=native -flto=auto <all .cpp files> -o engine
# g++ -std=c++14 -O3 -march=native -flto=auto magic.cpp pst.cpp uci.cpp state_2.cpp -o engine


