test: test.c logtee.h
	$(CC) test.c -o test && ./test && true
