CC     = gcc
CFLAGS = -Wall
SRC    = *.c
OUT    = sfs


$(OUT):$(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)
