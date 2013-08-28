CXXFILES	:= $(wildcard *.cpp) $(wildcard dir/*.cpp) $(wildcard interface/*.cpp)
CFILES		:= 

HEADERS		:= $(wildcard include/*.h) include/buildsys.h.gch

CPPFLAGS	:= -Iinclude -D_GNU_SOURCE
CXXFLAGS	:= -Wall -Werror -ggdb2 `pkg-config --cflags lua` $(USER_DEFINES)
CFLAGS		:= -std=c99 $(CXXFLAGS)
LDFLAGS		:= `pkg-config --libs lua` -lrt

UNDERSCORE_PATH	?=../underscore


ifeq ($(UNDERSCORE),y)
	CFILES += $(addprefix $(UNDERSCORE_PATH)/lib/, \
				$(addprefix datacoding/, datacoding.c) \
				$(addprefix lock/, lock.c cond.c) \
				$(addprefix tree/, avl.c bst.c heap.c mbtree.c splay.c) \
				$(addprefix thread/, thread.c) \
				$(addprefix encryption/, rc4.c) \
				$(addprefix event/, event_handling.c event_main.c event_set.c) \
				$(addprefix queue/, fifo.c) \
				$(addprefix jq/, jq_main.c) \
				$(addprefix network/, net_connect.c net_helpers.c net_listen.c \
							net_recv.c net_send.c) \
				$(addprefix delay/, delay_main.c) \
				$(addprefix _core/, core_main.c) \
				$(addprefix client/, client_main.c))
	UCPPFLAGS += $(addprefix -I$(UNDERSCORE_PATH)/lib/, \
				common/include/ datacoding/include/ \
				lock/include/ tree/include/ \
				thread/include/ encryption/include/ \
				event/include/ queue/include/ \
				jq/include/ network/include/ \
				delay/include/ _core/include/ client/include/) \
				-DUNDERSCORE
	CXXFLAGS += $(UCPPFLAGS)
	CFLAGS += $(UCPPFLAGS)
	LDFLAGS += -lz
endif

OBJS		:= $(CXXFILES:.cpp=.o) $(CFILES:.c=.o)


all: buildsys

$(OBJS): Makefile $(HEADERS)

%.h.gch : %.h
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

buildsys: Makefile $(OBJS)
	$(LINK.cpp) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -f $(OBJS) buildsys
