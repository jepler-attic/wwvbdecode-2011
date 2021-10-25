CXX := g++
CFLAGS := -Os -g -Wall

wwvbdecode: wwvbdecode.cc
	$(CXX) $(CFLAGS) -o $@ $<
