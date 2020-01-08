#include <unistd.h>
#include "rtmp_dump.h"

void Using() {
  debug("Using rtmp h264file aacfile, opusfile");
  _exit(0);
}

int main(int argc, const char * argv[]) {
      if (argc < 5) {
        Using();
      }
      RtmpDump(argv[1], argv[2], argv[3], argv[4]);
      sleep(1);
      return 0;
}
