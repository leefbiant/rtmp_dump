#include <unistd.h>
#include "rtmp_dump.h"

void Using() {
  debug("Using rtmp h264file aacfile");
  _exit(0);
}

int main(int argc, const char * argv[]) {
      if (argc < 4) {
        Using();
      }
      RtmpDump(argv[1], argv[2], argv[3]);
      return 0;
}
