all: hotel
hotel: main.cpp
	g++ main.cpp -o hotel -lncurses -pthread
