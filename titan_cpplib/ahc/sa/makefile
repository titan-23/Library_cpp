CXX = g++
CXXFLAGS = -O2 -march=native -I./../../../../Library_cpp -I. -std=c++20 -fopenmp
# -fsanitize=address

OUT = a.out

default:
	@echo "Usage: make <target>"

%: %.cpp
	$(CXX) $(CXXFLAGS) -o $(OUT) $<

clean:
	rm $(OUT)
