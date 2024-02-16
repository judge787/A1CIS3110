CC=gcc
CFLAGS=-Wall -g -std=c11
TARGET=A1

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET) *.hist *.o