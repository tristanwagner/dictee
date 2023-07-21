dictee: ./*.c ./*h
	gcc ./*.c -Linclude/utils -lutils -Iinclude/utils/src/ -o dictee -std=c99 -x objective-c -O0 -w
update-submodules:
	git submodule update --remote --recursive
utils:
	./include/utils/build/osx/build-lib.sh
clean:
	rm -f dictee
