CFLAGS = -Wall -g

PORT = 

IP_SERVER = 

ID_SUBSCRIBER =

all: server subscriber

# Compileaza server.c
server: server.c

# Compileaza subscriber.c
subscriber: subscriber.c

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server: server
	./server ${PORT}

# Ruleaza clientul TCP
run_subscriber: subscriber
	./subscriber ${ID_SUBSCRIBER} ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber
