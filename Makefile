CXXFILES	:= $(wildcard *.cpp) $(wildcard dir/*.cpp) $(wildcard interface/*.cpp) $(wildcard extraction/*.cpp)
CFILES		:= 

HEADERS		:= $(wildcard include/*.h) include/buildsys.h.gch

CPPFLAGS	:= -I. -D_GNU_SOURCE
LUAVERSION := $(shell pkg-config --exists lua && echo lua || (pkg-config --exists lua5.2 && echo lua5.2 || echo none))
ifeq ($(LUAVERSION),none)
$(error Can't find lua, please install and/or check that pkg-config knows about it)
endif
WARNFLAGS	:= -Werror -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wcast-align -Wunused -Woverloaded-virtual \
		   -Wpedantic -Wconversion -Wsign-conversion -Wlogical-op -Wuseless-cast -Wdouble-promotion \
		   -Wold-style-cast
BASEFLAGS	:= $(WARNFLAGS) -ggdb2 -pthread $(shell pkg-config --cflags $(LUAVERSION)) $(USER_DEFINES)
CXXFLAGS	:= -std=c++14 $(BASEFLAGS)
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
	find -name \*.cpp -not -path "*test/*" -exec clang-format -i -style=file {} \;
	find -name \*.h -not -path "*test/*" -exec clang-format -i -style=file {} \;

cpplint:
	find -name \*.cpp -not -path "*test/*" -exec cpplint {} \;
	find -name \*.h -not -path "*test/*" -exec cpplint {} \;

tidy:
	find -name \*.cpp -not -path "*test/*" -exec clang-tidy \
		-checks=*,-fuchsia-default-arguments,-hicpp-vararg,-cppcoreguidelines-pro-type-vararg,-cert-env33-c,-llvm-header-guard \
		-quiet {} -- $(CPPFLAGS) $(shell pkg-config --cflags $(LUAVERSION)) \;
	find -name \*.h -not -path "*test/*" -exec clang-tidy \
		-checks=*,-fuchsia-default-arguments,-hicpp-vararg,-cppcoreguidelines-pro-type-vararg,-cert-env33-c,-llvm-header-guard \
		-quiet -extra-arg-before=-xc++ {} -- $(CPPFLAGS) $(shell pkg-config --cflags $(LUAVERSION)) \;

.PHONY: test
