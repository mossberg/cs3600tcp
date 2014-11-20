SEND = 3600send
RECV = 3600recv
SENDRECV = 3600sendrecv

all: $(SEND) $(RECV)

$(SENDRECV).o: $(SENDRECV).c
	gcc -c -std=c99 -O0 -g -lm -Wall -pedantic -Wextra -o $@ $<

$(SEND): $(SEND).c $(SENDRECV).o
	gcc -std=c99 -O0 -g -lm -Wall -pedantic -Wextra -o $@ $< $(SENDRECV).o

$(RECV): $(RECV).c $(SENDRECV).o
	gcc -std=c99 -O0 -g -lm -Wall -pedantic -Wextra -o $@ $< $(SENDRECV).o

test: all
	./test

clean:
	rm $(SEND) $(RECV) $(SENDRECV).o

