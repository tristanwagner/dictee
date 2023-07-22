FLAGS= -std=c99 -x objective-c -O0 -w
UTILS_PATH=./include/utils
SRCS := $(wildcard ./*.c)
OBJS := $(SRCS:.c=.o)
BINS= dictee dictee_shared

all: clean static

static: libutils.a dictee

shared: libutils.so dictee_shared

# using the static lib utils
dictee: dictee.c editor.c
	$(CC) $(FLAGS) -o $@ $^ -L$(UTILS_PATH) -lutils

update-submodules:
	git submodule update --remote --recursive

libutils.a: $(UTILS_PATH)/src/*
	$(MAKE) libutils.a -C $(UTILS_PATH)

# using the shared/dynamic lib utils
# TODO make something more cross platform
dictee_shared: dictee.c editor.c
	$(CC) $(FLAGS) -o $@ $^ -L$(UTILS_PATH) -lutils
	install_name_tool -change libutils.so @executable_path/include/utils/libutils.so $@

libutils.so: $(UTILS_PATH)/src/*
	$(MAKE) libutils.so -C $(UTILS_PATH)

clean:
	rm -f $(BINS) $(OBJS)
	$(MAKE) clean -C $(UTILS_PATH)
