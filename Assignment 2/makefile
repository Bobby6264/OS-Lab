CC = gcc
CFLAGS = -Wall
TARGETS = parent child dummy

all: $(TARGETS)

parent: parent.c
	$(CC) $(CFLAGS) -o parent parent.c

child: child.c
	$(CC) $(CFLAGS) -o child child.c

dummy: dummy.c
	$(CC) $(CFLAGS) -o dummy dummy.c

run: all
	./parent 10

clean:
	-rm -f $(TARGETS) childpid.txt dummycpid.txt
