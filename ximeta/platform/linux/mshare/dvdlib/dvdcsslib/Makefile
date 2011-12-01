SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c, %.o , $(SRCS))


all: $(OBJS)
	$(MSHARE-LD) $(MSLDFLAGS) -o dvdcsslib.o -r  $(OBJS)

%.o : %.c
	$(MSHARE-CC) $(MSCFLAGS) -c -o $@ $<

clean :
	rm -f $(OBJS)
