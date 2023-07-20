dictee: ./*.c ./*h
		gcc ./*.c ./include/utils/src/*.c -o dictee -std=c99 -x objective-c -O0 -w
