#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h" // Needed for the macros

int
main(int argc, char *argv[])
{
  struct stat st;

  if(argc < 2){
    fprintf(2, "Usage: stat pathname\n");
    exit(1);
  }

  if(stat(argv[1], &st) < 0){
    fprintf(2, "stat: cannot stat %s\n", argv[1]);
    exit(1);
  }

  printf("File: %s\n", argv[1]);
  printf("Type: %d\n", st.type);
  printf("Size: %lu\n", st.size);

  if(st.type == T_EXTENT) {
    printf("--- Extents ---\n");
    for(int i = 0; i < 12; i++) { // Only check direct blocks
      uint entry = st.addrs[i];
      
      if(entry == 0) continue;

      uint addr = EXTENT_ADDR(entry);
      uint len  = EXTENT_LEN(entry);
      
      printf("Slot %d: Start Block %d, Length %d\n", i, addr, len);
    }
  } else {
    printf("--- Standard Blocks ---\n");
    // Optional: print standard blocks for T_FILE
  }

  exit(0);
}
