CXX=g++
CFLAGS=-I
skiplist: main.o
		$(CXX) -o ./bin/main main.o --std=c++11 -pthread
		rm -f ./*.o

clean:
		rm -rf ./*.o