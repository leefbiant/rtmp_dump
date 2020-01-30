CC = g++
LIB = -lrtmp -lopus -lfaad -lfaac -logg -lsamplerate -l:libmp4v2.a -lssl -lcrypto -lz 
RUNLIBPATH=/opt/source/rtmpdump/librtmp
LIBPATH=-L../rtmpdump/librtmp -L/opt/source/lib/lib
CINC = -I/opt/source/rtmpdump/librtmp -I/opt/source/lib/include
CXXFLAGS = -Wall -Wno-reorder -Wno-format -g -O0  $(CINC)
LDFLAGS = -Wl,-rpath,$(RUNLIBPATH)

target = rtmp_dump

objects = rtmp_dump.o main.o flvparse.o audio_codec.o ogg_packet.o mp4_encode.o sps_pps.o flvdump.o 
all: $(target)
$(target): $(objects)
	$(CC) -o $@ $^ $(LIBPATH) $(LIB) $(LDFLAGS) 
$(objects): %.o: %.cpp
	$(CC) -c $(CXXFLAGS) $< -o $@
clean:
	rm -rf *.o $(target)
