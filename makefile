CC = gcc
CFLAGS = -c -g
LDFLAGS = -no-pie
OBJECTS = testcase.o disk.o hw1.o hw2.o

hw2: $(OBJECTS)
	$(CC) $(LDFLAGS) -o hw2 $(OBJECTS)

disk.o : disk.c
	$(CC) $(CFLAGS) disk.c

hw1.o : hw1.c
	$(CC) $(CFLAGS) hw1.c

hw2.o : hw2.c
	$(CC) $(CFLAGS) hw2.c

testcase.o : testcase.c
	$(CC) $(CFLAGS) testcase.c
