CXX = g++
CXXFLAGS = -std=c++11 -pthread

all: ImageFilters.out

ImageFilters.out: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o ImageFilters.out

clean:
	rm -f ImageFilters.out