obj-m += sys_xdedup.o

INC=/lib/modules/$(shell uname -r)/build/arch/x86/include

all: xhw1 making

xhw1: xhw1.c
	gcc -Wall -Werror -I$(INC)/generated/uapi -I$(INC)/uapi xhw1.c -o xdedup

making:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f xdedup
