SRCS=IT8951.c main.c bcm2835.c
CC=gcc
TARGET=IT8951

$(TARGET):$(SRCS)
	$(CC) -g -ggdb -O3 -Wall $(SRCS) -o $(TARGET)
	
clean:
	rm -f $(TARGET)
all:$(TARGET)