CXXFILES	:= $(wildcard src/*.cpp) $(wildcard src/dir/*.cpp) $(wildcard src/interface/*.cpp) $(wildcard src/extraction/*.cpp)
CFILES		:= 

HEADERS		:= $(wildcard src/include/*.h)

CPPFLAGS	:= -Isrc/ -D_GNU_SOURCE
LUAVERSION := $(shell pkg-config --exists lua && echo lua || (pkg-config --exists lua5.2 && echo lua5.2 || echo none))
ifeq ($(LUAVERSION),none)
$(error Can't find Lua development support. Please install Lua development support \
(liblua-dev (deb) or lua-devel (rpm)) and check that pkg-config knows about Lua development)
endif
WARNFLAGS	:= -Werror -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wcast-align -Wunused -Woverloaded-virtual \
		   -Wpedantic -Wconversion -Wsign-conversion -Wlogical-op -Wuseless-cast -Wdouble-promotion \
		   -Wold-style-cast -Wno-missing-field-initializers
BASEFLAGS	:= $(WARNFLAGS) -ggdb2 -pthread $(shell pkg-config --cflags $(LUAVERSION)) $(USER_DEFINES)
CXXFLAGS	:= -std=c++17 $(BASEFLAGS)
CFLAGS		:= -std=c99 $(BASEFLAGS)
LDFLAGS		:= $(shell pkg-config --libs $(LUAVERSION)) -lrt -pthread -lssl -lcrypto -lutil -lstdc++fs

OBJS		:= $(CXXFILES:.cpp=.o) $(CFILES:.c=.o)


all: buildsys

$(OBJS): Makefile $(HEADERS)

buildsys: Makefile $(OBJS)
	$(LINK.cpp) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -f $(OBJS) buildsys

.PHONY: test
