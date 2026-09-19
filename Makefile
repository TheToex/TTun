CC = gcc
CFLAGS = -Wall -O2
TARGET = ttun-core
SRC = src/ttun-core.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

install: $(TARGET)
	@echo "Installing TTun..."
	install -Dm755 $(TARGET) /usr/bin/$(TARGET)
	install -Dm755 bin/ttun /usr/bin/ttun
	install -Dm644 systemd/ttun@.service /etc/systemd/system/ttun@.service
	mkdir -p /etc/ttun
	systemctl daemon-reload
	@echo "TTun installed successfully!"

clean:
	rm -f $(TARGET)