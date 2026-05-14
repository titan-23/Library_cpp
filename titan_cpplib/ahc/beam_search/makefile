CXX = g++
CXXFLAGS = -O2 -march=native -I./../../../../Library_cpp -I. -std=c++20 -DLOCAL -fopenmp
#  -fsanitize=address

OUT = a.out

default:
	@echo "Usage: make <target>"

%: %.cpp
	$(CXX) $(CXXFLAGS) -o $(OUT) $<

clean:
	rm $(OUT)


# find /usr/include -name "stdc++.h" | grep bits
# sudo g++ -O2 -march=native -std=c++20 -DLOCAL -fopenmp /usr/include/x86_64-linux-gnu/c++/13/bits/stdc++.h
# sudo rm /usr/include/x86_64-linux-gnu/c++/13/bits/stdc++.h.gch
# ls -l /usr/include/x86_64-linux-gnu/c++/13/bits/stdc++.h.gch
