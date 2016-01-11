CXXFILES	:= $(wildcard *.cpp) $(wildcard dir/*.cpp) $(wildcard interface/*.cpp)
CFILES		:= 

HEADERS		:= $(wildcard include/*.h) include/buildsys.h.gch

CPPFLAGS	:= -Iinclude -D_GNU_SOURCE
LUAVERSION := $(shell pkg-config --exists lua && echo lua || (pkg-config --exists lua5.2 && echo lua5.2 || echo none))
ifeq ($(LUAVERSION),none)
$(error Can't find lua, please install and/or check that pkg-config knows about it)
endif
BASEFLAGS	:= -Wall -Werror -ggdb2 -pthread $(shell pkg-config --cflags $(LUAVERSION)) $(USER_DEFINES)
CXXFLAGS	:= -std=c++11 $(BASEFLAGS)
CFLAGS		:= -std=c99 $(BASEFLAGS)
LDFLAGS		:= $(shell pkg-config --libs $(LUAVERSION)) -lrt -pthread

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

INDENT_ARGS=-kr -i8 -ts8 -l92 -nsai -nsaf -nsaw -il8
indent:
	find -name \*.cpp -exec indent $(INDENT_ARGS) {} \;
	indent $(INDENT_ARGS) include/buildsys.h

.PHONY: test
