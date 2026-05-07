CXX = g++
CXX_FLAGS = -std=c++20 -Wall -O3 -march=native -flto

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

# g++ -std=c++20 -O3 -march=native -flto <all .cpp files> -o chess