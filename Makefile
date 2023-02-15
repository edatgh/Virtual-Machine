CFLAGS += -Wall
CFLAGS += -ggdb
OBJS = cpu.o io.o mem.o asm.o main.o
TARGET = vm

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) -o $@ $^

clean:
	- rm -f $(OBJS) $(TARGET)
