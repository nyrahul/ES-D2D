SRCS=$(wildcard src/*.c)
TARGET=rawpkt

all:
	$(CC) $(SRCS) -o $(TARGET)

clean:
	@rm -f $(TARGET)

