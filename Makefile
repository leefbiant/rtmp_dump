CC = gcc
LIB = -lrtmp -lssl -lcrypto -lz 
RUNLIBPATH=/opt/source/rtmpdump/librtmp
LIBPATH=-L../rtmpdump/librtmp 
CINC = -I/opt/source/rtmpdump/librtmp
CFLAGS = -Wall -g -O2  $(CINC) -Wimplicit-function-declaration
LDFLAGS = -Wl,-rpath,$(RUNLIBPATH)

target = rtmp_h264_aac

objects = rtmp_h264_aac.o main.o flvparse.o
all: $(target)
$(target): $(objects)
	$(CC) -o $@ $^ $(LIBPATH) $(LIB) $(LDFLAGS) 
$(objects): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
clean:
	rm -rf *.o $(target)
