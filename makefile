CC=gcc

SDIR=src
FLAGS = -lpthread

_DEPS = := $(wildcard $(SDIR)/*.h)
DEPS = $(patsubst %,$(SDIR)/%,$(_DEPS))

SRC := $(wildcard $(SDIR)/*.c)
#SRC= $(patsubst %,$(SDIR)/%,$(_SRC))

event: $(SRC)
	$(CC) -o $@ $^ $(FLAGS)
