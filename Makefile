EXEC = netlog
LIB = -lm
all: $(EXEC)

$(EXEC): netlog.o
	gcc -o $@ $^ $(LIB)

netlog.o: netlog.c
	gcc -c -o $@ $^

clean:
	rm -f *.o $(EXEC)
