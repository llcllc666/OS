// user/mkfile.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#ifndef O_EXTENT
#define O_EXTENT 0x800
#endif

#ifndef O_INLINE
#define O_INLINE 0x1000
#endif

int
main(int argc, char *argv[])
{
  int flags = O_CREATE | O_RDWR; // Default flags
  char *path;

  if(argc < 3){
    fprintf(2, "Usage: mkfile -e|-i filename\n");
    exit(1);
  }

  if(strcmp(argv[1], "-e") == 0){
    flags |= O_EXTENT; // Add the Extent flag
    path = argv[2];
  } 
  else if(strcmp(argv[1], "-i") == 0){
    flags |= O_INLINE; // Add the Inline flag
    path = argv[2];
  } 
  else {
    fprintf(2, "Invalid flag. Use -e (extent) or -i (inline)\n");
    exit(1);
  }

  int fd = open(path, flags);
  if(fd < 0){
    fprintf(2, "Failed to create %s\n", path);
    exit(1);
  }

  close(fd);
  exit(0);
}