CC=gcc

all: selectqkiller

selectqkiller: selectqkiller.c
	$(CC) selectqkiller.c $(shell mysql_config --libs)  $(shell mysql_config --cflags) $(shell pkg-config --libs --cflags glib-2.0) -o selectqkiller

clean:
	rm -rf selectqkiller

