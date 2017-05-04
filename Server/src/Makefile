SRC=$(wildcard *.c)
OBJ=$(SRC:%.c=%.o)
APP=rtsp
$SRC_UTILS=$(wildcard ../utils/*.c)
OBJ_UTILS=$(SRC_UTILS:%.c=%.o)

CFLAGS=-I../include -I. -I./sensor/ 

all: $(APP)
$(APP) : $(OBJ_UTILS) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lrt -lm


$(OBJ) : %.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(OBJ_UTILS) : %.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@


	rm -f $(OBJ)

.PHONY: clean
clean:
	@rm -f $(APP)
	@rm -f $(OBJ)

