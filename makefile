cxx = g++
cxxflags = -std=c++17 -Wall -O2 -Wno-sign-compare
ldlibs = -lsqlite3 -ljsoncpp -lcurl -lzip
target = fboxbashdl_backend
srcs = $(wildcard *.cpp)

all: $(target)

$(target): $(srcs)
	$(cxx) $(cxxflags) -o $(target) $(srcs) $(ldlibs)

clean:
	rm -f $(target)
