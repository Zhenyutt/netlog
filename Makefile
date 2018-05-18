EXEC = netlog
all: $(EXEC)

$(EXEC): netlog.o
	gcc -o $@ $^

netlog.o: netlog.c
	gcc -c -o $@ $^

clean:
	rm -f *.o $(EXEC)
