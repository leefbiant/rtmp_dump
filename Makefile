CC = g++
LIB = -lrtmp -lssl -lcrypto -lz 
RUNLIBPATH=/opt/source/rtmpdump/librtmp
LIBPATH=-L../rtmpdump/librtmp 
CINC = -I/opt/source/rtmpdump/librtmp
CXXFLAGS = -Wall -g -O2  $(CINC) -Wimplicit-function-declaration
LDFLAGS = -Wl,-rpath,$(RUNLIBPATH)

target = rtmp_dump

objects = rtmp_dump.o main.o flvparse.o
all: $(target)
$(target): $(objects)
	$(CC) -o $@ $^ $(LIBPATH) $(LIB) $(LDFLAGS) 
$(objects): %.o: %.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@
clean:
	rm -rf *.o $(target)
