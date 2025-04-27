all:
	gcc -std=c11 -pedantic -pthread crawler.c -lcurl -o crawler 
clean:
	rm -f crawler page*.html 
run: 
	./crawler