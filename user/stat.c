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
  } else if(st.type == T_INLINE) { // Type 5
     printf("Type: Inline File\n");
     printf("Size: %l\n", st.size);
     
     // Print the data directly since it's small
     printf("Data Content: \"");
     char *data = (char*)st.addrs;
     for(int i=0; i<st.size; i++) {
         printf("%c", data[i]);
     }
     printf("\"\n");
  }
  else {
     printf("--- Standard Blocks ---\n");
  }
  
  exit(0);
}
