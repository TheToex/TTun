CC=gcc
CFLAGS=-Wall -O2
TARGET=ttun

all: $(TARGET)

$(TARGET): ttun.c
	$(CC) $(CFLAGS) -o $(TARGET) ttun.c

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)

clean:
	rm -f $(TARGET)