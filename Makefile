CC = g++
LIB = -lrtmp -lopus -lfaad -logg -lsamplerate -lssl -lcrypto -lz 
RUNLIBPATH=/opt/source/rtmpdump/librtmp
LIBPATH=-L../rtmpdump/librtmp -L/opt/source/lib/lib
CINC = -I/opt/source/rtmpdump/librtmp -I/opt/source/lib/include
CXXFLAGS = -Wall -g -O0  $(CINC) -Wimplicit-function-declaration
LDFLAGS = -Wl,-rpath,$(RUNLIBPATH)

target = rtmp_dump

objects = rtmp_dump.o main.o flvparse.o opus_encode.o ogg_packet.o
all: $(target)
$(target): $(objects)
	$(CC) -o $@ $^ $(LIBPATH) $(LIB) $(LDFLAGS) 
$(objects): %.o: %.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@
clean:
	rm -rf *.o $(target)
