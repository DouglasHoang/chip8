CC = gcc
FLAGS = -Wall
STATIC = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

build:
	$(CC) $(FLAGS) main.c -o main $(STATIC)
