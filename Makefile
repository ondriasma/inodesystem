CC=gcc
CFLAGS = -Wall -Wextra -pedantic

all:	clean comp

comp:
	${CC} commandline.c main.c filesystem.c inodes.c clusters.c -o zos_vfs -lpthread -lm -Wall


clean:
	rm -f zos_vfs
	rm -f *.*~
