dictee: ./*.c ./*h
		$(CC) ./*.c -o dictee -Wall -Wextra -pedantic -std=c99 -g
