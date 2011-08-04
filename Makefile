CXX_FILES:=main.cpp interface.cpp world.cpp package.cpp packagecmd.cpp run.cpp \
		$(addprefix dir/, dir.cpp builddir.cpp)
CFILES:= 

HEADERS:=include/*.h
CXXFLAGS:=-Wall -ggdb2 -Iinclude `pkg-config --cflags --libs lua` -lrt $(USER_DEFINES)

ifeq ($(UNDERSCORE),y)
	CXXFLAGS+= $(addprefix -I$(UNDERSCORE_PATH)/lib/, \
				common/include/ datacoding/include/ \
				lock/include/ tree/include/ \
				thread/include/ encryption/include/ \
				event/include/ queue/include/ \
				jq/include/ network/include/ \
				delay/include/ _core/include/ client/include/) \
				-DUNDERSCORE
	CXX_FILES+= $(addprefix lm/, linux.c)
	CFILES+= $(addprefix $(UNDERSCORE_PATH)/lib/, \
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
	LDFLAGS+= -lz
endif

COBJS:= $(CFILES:.c=.o)


all: buildsys

%.o: %.c Makefile
	gcc -c -o $(@) $(<) $(CXXFLAGS) -std=c99 -D_GNU_SOURCE

buildsys: $(CXX_FILES) $(COBJS) Makefile $(HEADERS)
	g++ -o $(@) $(CXX_FILES) $(CXXFLAGS) $(COBJS) $(LDFLAGS)

clean:
	rm -f $(COBJS) buildsys

