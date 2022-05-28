VERSION = 1.0.1
TARGET  = imobax
OBJS    = $(wildcard src/*.c)
FLAGS  ?= -Wall -O3 -DVERSION=$(VERSION) -DTIMESTAMP="`date +'%d. %B %Y %H:%M:%S'`" -flto -lsqlite3 $(CFLAGS)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(FLAGS)

clean:
	rm -f $(TARGET)
