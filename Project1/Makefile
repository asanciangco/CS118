CC=gcc
CFLAGS=-I.
DEPS = # header file 
OBJ = serverFork.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

serverFork: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)


