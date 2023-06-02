all: main

main: 
	gcc -pthread main.c -o findeq
	clear

clean:
	rm -rf ./findeq
	clear