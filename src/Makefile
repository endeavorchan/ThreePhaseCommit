NODE = threepc.cpp msg.cpp
CPPFLAGS=-O2 -std=c++11

all: threepc

threepc: $(NODE)
	g++ $(CPPFLAGS) -o ../bin/threepc $(NODE)

clean:
	rm -f *~
	rm -f ../bin/threepc

