SRCS=$(wildcard src/*.c)
TARGET=rawpkt

all:
	$(CC) $(SRCS) -o $(TARGET) -lpthread

clean:
	@rm -f $(TARGET)

