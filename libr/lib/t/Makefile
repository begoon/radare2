BINDEPS=r_lib r_util
LDFLAGS=-L..

include ../../config.mk

all: plugin.${EXT_SO} test-plugin${EXT_EXE}

plugin.${EXT_SO}:
	${CC} ${CFLAGS} -shared -Wall -o plugin.${EXT_SO} ${LDFLAGS_LIB} plugin.c -I../../include

include ../../rules.mk

clean::
	-rm -f plugin.${EXT_SO} test-plugin${EXT_EXE}
