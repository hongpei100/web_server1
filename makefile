all:webserver
	gcc webserver.c -o webserver
clean:
	rm -f webserver
