CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -g
OBJ = main.o tcp.o udp.o

OWR: $(OBJ)
	$(CC) $(CFLAGS) -o OWR $(OBJ)

main.o: main.c tcp.h udp.h
	$(CC) $(CFLAGS) -c main.c

tcp.o: tcp.c tcp.h
	$(CC) $(CFLAGS) -c tcp.c

udp.o: udp.c udp.h tcp.h
	$(CC) $(CFLAGS) -c udp.c

clean:
	rm -f *.o OWR