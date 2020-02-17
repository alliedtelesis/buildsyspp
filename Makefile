CXXFILES	:= $(wildcard *.cpp) $(wildcard dir/*.cpp) $(wildcard interface/*.cpp) $(wildcard extraction/*.cpp)
CFILES		:= 

HEADERS		:= $(wildcard include/*.h) include/buildsys.h.gch

CPPFLAGS	:= -I. -D_GNU_SOURCE
LUAVERSION := $(shell pkg-config --exists lua && echo lua || (pkg-config --exists lua5.2 && echo lua5.2 || echo none))
ifeq ($(LUAVERSION),none)
$(error Can't find lua, please install and/or check that pkg-config knows about it)
endif
WARNFLAGS	:= -Werror -Wall -Wextra -Wshadow
BASEFLAGS	:= $(WARNFLAGS) -ggdb2 -pthread $(shell pkg-config --cflags $(LUAVERSION)) $(USER_DEFINES)
CXXFLAGS	:= -std=c++11 $(BASEFLAGS)
CFLAGS		:= -std=c99 $(BASEFLAGS)
LDFLAGS		:= $(shell pkg-config --libs $(LUAVERSION)) -lrt -pthread -lssl -lcrypto

OBJS		:= $(CXXFILES:.cpp=.o) $(CFILES:.c=.o)


all: buildsys

$(OBJS): Makefile $(HEADERS)

%.h.gch : %.h
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

buildsys: Makefile $(OBJS)
	$(LINK.cpp) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -f $(OBJS) buildsys

test:
	$(MAKE) -C test

indent:
	find -name \*.cpp -exec clang-format -i -style=file {} \;
	clang-format -i -style=file include/buildsys.h

cpplint:
	find -name \*.cpp -exec cpplint {} \;
	cpplint include/buildsys.h

.PHONY: test
