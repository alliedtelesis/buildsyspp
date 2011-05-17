CXX_FILES:=main.cpp interface.cpp world.cpp package.cpp packagecmd.cpp run.c \
		$(addprefix dir/, dir.cpp builddir.cpp)

HEADERS:=include/*.h
CXXFLAGS:=-Wall -Werror -ggdb2 -Iinclude `pkg-config --cflags --libs lua` -lrt

all: buildsys

buildsys: $(CXX_FILES) Makefile $(HEADERS)
	g++ -o $(@) $(CXX_FILES) $(CXXFLAGS)

