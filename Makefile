CXX = g++
CXXFLAGS = -Wall -Wextra
TARGETS = networkMonitor intfMonitor

all: $(TARGETS)

networkMonitor: networkMonitor.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

intfMonitor: intfMonitor.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) *.o
