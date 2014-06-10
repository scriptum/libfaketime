CFLAGS += -fPIC -Wall -Wextra -O2 -s

NAME=faketime

LIB64 = lib$(NAME)64.so
LIB32 = lib$(NAME)32.so

LDFLAGS += -ldl -lpthread -lX11 -shared

all: $(LIB64) $(LIB32)

$(LIB64): $(NAME).c libpreload.h
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(LIB32): $(NAME).c libpreload.h
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -m32

clean:
	-rm -f *.so
