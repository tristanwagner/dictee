UTILS_PATH=./include/utils
LIBS=-I${UTILS_PATH}/src -L$(UTILS_PATH) -lutils
FLAGS= -std=c99 -O0 -w
FLAGS_OSX= $(FLAGS) -framework Cocoa
SRCS := $(wildcard ./*.c)
OBJS := $(SRCS:.c=.o)
BINS= dictee dictee_dbg dictee_shared

# all: clean static
all: clean static_osx shared_osx

static_osx: libutils.a dictee_osx dictee_dbg_osx

shared_osx: libutils.dylib dictee_shared_osx

static: libutils.a dictee dictee_dbg

shared: libutils.so dictee_shared

# using the static lib utils
dictee: dictee.c editor.c
	$(CC) $(FLAGS) $(LIBS) -o $@ $^

dictee_dbg: dictee.c editor.c
	$(CC) $(FLAGS) $(LIBS) -g -o $@ $^

dictee_osx: dictee.c editor.c
	$(CC) $(FLAGS_OSX) $(LIBS) -o dictee $^

dictee_dbg_osx: dictee.c editor.c
	$(CC) $(FLAGS_OSX) $(LIBS) -g -o dictee_dbg $^

update-submodules:
	git submodule update --remote --recursive

libutils.a:
	$(MAKE) $@ -C $(UTILS_PATH)

# using the shared/dynamic lib utils
# TODO make something more cross platform
dictee_shared: dictee.c editor.c libutils.so
	$(CC) $(FLAGS) -o $@ $?

dictee_shared_osx: dictee.c editor.c libutils.dylib
	$(CC) $(FLAGS_OSX) -I${UTILS_PATH}/src -o dictee_shared $?

libutils.so:
	$(MAKE) $@ -C $(UTILS_PATH)

libutils.dylib:
	$(MAKE) $@ -C $(UTILS_PATH)
	cp $(UTILS_PATH)/$@ ./$@

clean:
	rm -f $(BINS) $(OBJS) *.dylib
	rm -rf *.dSYM
	$(MAKE) clean -C $(UTILS_PATH)

%.m.o: %.m
	$(CC) -c $< -o $@
