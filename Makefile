CXXFLAGS=`wx-config --cxxflags` -std=c++17
LDFLAGS=`wx-config --libs`

main: main.o
	$(CXX) $(LDFLAGS) main.o $(LOADLIBES) $(LDLIBS) -o main
main.o: main.cpp adaptivecards-wx.h
	$(CXX) $(CXXFLAGS) main.cpp -c -o main.o

wrapsizer: wrapsizer.o
	$(CXX) $(LDFLAGS) wrapsizer.o $(LOADLIBES) $(LDLIBS) -o wrapsizer

wrapsizer.o: wrapsizer.cpp
	$(CXX) $(CXXFLAGS) wrapsizer.cpp -c -o wrapsizer.o
