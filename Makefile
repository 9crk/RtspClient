SRC=$(wildcard *.c)
OBJ=$(SRC:%.c=%.o)
APP=main
$SRC_UTILS=$(wildcard ../utils/*.c)
OBJ_UTILS=$(SRC_UTILS:%.c=%.o)

CFLAGS=-I../utils -I. -Iinclude -lpthread -lrt -lm

all: $(APP)
$(APP) : $(OBJ_UTILS) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ 

$(OBJ) : %.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(OBJ_UTILS) : %.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@


	rm -f $(OBJ)

.PHONY: clean
clean:
	@rm -f $(APP)
	@rm -f $(OBJ)

