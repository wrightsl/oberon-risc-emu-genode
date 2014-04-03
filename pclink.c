// pclink.c for Peter De Wachter's RISC emulator PDR 20.3.14
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "pclink.h"

#define ACK 0x10
#define REC 0x21
#define SND 0x22

#ifndef S_IRGRP
#define S_IRGRP 0
#endif

#ifndef S_IWGRP
#define S_IWGRP 0
#endif

#ifndef S_IROTH
#define S_IROTH 0
#endif

#ifndef S_IWOTH
#define S_IWOTH 0
#endif

static const char * RecName = "PCLink.REC";  // e.g. echo Test.Mod > PCLink.REC
static const char * SndName = "PCLink.SND";
static uint8_t mode = 0;
static int fd = -1;
static int txcount, rxcount, fnlen, flen;
static char szFilename[32];
static char buf[257];

static bool GetJob(const char *JobName) {
  bool res = false;
  struct stat st;
  FILE * f;

  if (stat(JobName, &st) == 0) {
    if (st.st_size > 0 && st.st_size <= 33) {
      f = fopen(JobName, "r");
      if (f) {
        fscanf(f, "%31s", szFilename);
        fclose(f);
        res = true; txcount = 0; rxcount = 0; fnlen = (int)strlen(szFilename)+1;
      }
    }
    if (!res) {
      unlink(JobName);  // clean up
    }
  }
  return res;
}

uint32_t PCLink_RStat() {
  struct stat st;

  if (!mode) {
    if (GetJob(RecName)) {
      if (stat(szFilename, &st) == 0 && st.st_size >= 0 && st.st_size < 0x1000000) {
        fd = open(szFilename, O_RDONLY);
        if (fd != -1) {
          flen = (int)st.st_size; mode = REC;
          printf("PCLink REC Filename: %s size %d\n", szFilename, flen);
        }
      }
      if (!mode) {
        unlink(RecName);  // clean up
      }
    } else if (GetJob(SndName)) {
      fd = open(szFilename, O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
      if (fd != -1) {
        flen = -1; mode = SND;
        printf("PCLink SND Filename: %s\n", szFilename);
      }
      if (!mode) {
        unlink(SndName);  // clean up
      }
    }
  }
  return 2 + (mode != 0);  // xmit always ready
}

uint32_t PCLink_RData() {
  uint8_t ch = 0;

  if (mode) {
    if (rxcount == 0) {
      ch = mode;
    } else if (rxcount < fnlen+1) {
      ch = szFilename[rxcount-1];
    } else if (mode == SND) {
      ch = ACK;
      if (flen == 0) {
        mode = 0; unlink(SndName);
      }
    } else {
      int pos = (rxcount - fnlen - 1) % 256;
      if (pos == 0) {
        if (flen > 255) {
          ch = 255;
        } else {
          ch = (uint8_t)flen;
        }
      } else {
        read(fd, &ch, 1);
        flen--;
        if (flen == 0) {
          mode = 0; unlink(RecName);
        }
      }
    }
  }

  rxcount++;
  return ch;
}

void PCLink_TData(uint32_t value) {
  if (mode) {
    if (txcount == 0) {
      if (value != ACK) {
        close(fd); fd = -1;
        if (mode == SND) {
          unlink(szFilename);  // file not found, delete file created
          unlink(SndName);  // clean up
        } else {
          unlink(RecName);  // clean up
        }
        mode = 0;
      }
    } else if (mode == SND) {
      int lim;

      int pos = (txcount-1) % 256;
      buf[pos] = (uint8_t)value;
      lim = (unsigned char)buf[0];
      if (pos == lim) {
        write(fd, buf+1, lim);
        if (lim < 255) {
          flen = 0; close(fd);
        }
      }
    }
  }
  txcount++;
}
