all:
	gcc -o server -DSERVER common.h server.h main.c common.c server.c
	gcc -o client -DCLIENT common.h server.h main.c common.c server.c
