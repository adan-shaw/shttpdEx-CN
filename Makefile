CC = gcc
CFLAGS = -Wall -O2

LIBS = -lpthread
TARGET = shttpd
TARGET_DEBUG = shttpd-debug
RM = rm -f



HEADFILE = -I./src
SRC=./src/*.c



all:$(OBJS)
	$(CC) -g3 -o $(TARGET_DEBUG) $(HEADFILE) $(LIBS) $(SRC)

release:
	$(CC) -DNDEBUG -o $(TARGET) $(HEADFILE)  $(LIBS) $(SRC)

clean:
	$(RM) $(TARGET) $(TARGET_DEBUG) 
