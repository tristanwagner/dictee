UTILS_PATH=./include/utils
FLAGS_LIBUTILS=-I${UTILS_PATH}/src -L$(UTILS_PATH) -lutils
FLAGS= -std=c99 -O0 -w ${FLAGS_LIBUTILS}
FLAGS_OSX= $(FLAGS) -x objective-c -framework Cocoa
SRCS := $(wildcard ./*.c)
OBJS := $(SRCS:.c=.o)
BINS= dictee dictee_dbg dictee_shared

# all: clean static
all: clean static_osx

static_osx: libutils.a dictee_osx dictee_dbg_osx

shared_osx: libutils.so dictee_shared_osx

static: libutils.a dictee dictee_dbg

shared: libutils.so dictee_shared

# using the static lib utils
dictee: dictee.c editor.c
	$(CC) $(FLAGS) -o dictee $^

dictee_dbg: dictee.c editor.c
	$(CC) $(FLAGS) -g -o dictee_dbg $^

dictee_osx: dictee.c editor.c
	$(CC) $(FLAGS_OSX) -o dictee $^

dictee_dbg_osx: dictee.c editor.c
	$(CC) $(FLAGS_OSX) -g -o dictee_dbg $^

update-submodules:
	git submodule update --remote --recursive

libutils.a:
	$(MAKE) libutils.a -C $(UTILS_PATH)

# using the shared/dynamic lib utils
# TODO make something more cross platform
dictee_shared: dictee.c editor.c
	$(CC) $(FLAGS) -o dictee_shared $^

dictee_shared_osx: dictee.c editor.c
	$(CC) $(FLAGS_OSX) -o dictee_shared $^
	install_name_tool -change libutils.so @executable_path/include/utils/libutils.so $@

libutils.so: $(UTILS_PATH)/src/*
	$(MAKE) libutils.so -C $(UTILS_PATH)

clean:
	rm -f $(BINS) $(OBJS)
	$(MAKE) clean -C $(UTILS_PATH)

%.m.o: %.m
	$(CC) -c $< -o $@
