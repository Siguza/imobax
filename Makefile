VERSION = 1.0.1
TARGET  = imobax
SRCDIR  = src
ifeq ($(shell uname -s), Linux)
	WARN = -Wall -Wno-unused-but-set-variable -Wno-unused-result
	LIBS = -flto -lsqlite3 -lssl -lcrypto
else
	WARN = -Wall
	LIBS = -flto -lsqlite3
endif

FLAGS  ?= $(WARN) -O3 -DVERSION=$(VERSION) -DTIMESTAMP="`date +'%d. %B %Y %H:%M:%S'`" $(LIBS) $(CFLAGS)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCDIR)/*.c
	$(CC) -o $@ $^ $(FLAGS)

clean:
	rm -f $(TARGET)
