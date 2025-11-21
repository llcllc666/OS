#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#ifndef O_EXTENT
#define O_EXTENT 0x800
#endif

void
bench_threshold(char *label, int flags)
{
  int start, end;
  int n_files = 2000;  // INCREASED: From 60 to 2000
  int file_size_blocks = 14; // The "Indirect Block" trigger point
  char buf[1024];
  char name[16] = "temp"; // Fixed name, we overwrite/delete repeatedly

  memset(buf, 'A', 1024);

  printf("Running %s Threshold Test (%d files)...\n", label, n_files);

  start = uptime();

  for(int i = 0; i < n_files; i++){
    // 1. Create
    int fd = open(name, O_CREATE | O_RDWR | flags);
    if(fd < 0){
        printf("Create failed at iter %d\n", i);
        exit(1);
    }

    // 2. Write 14 blocks
    // Standard FS: Block 13 triggers allocation of Indirect Block (Expensive)
    // Extent FS: Block 13 just extends the length (Cheap)
    for(int j = 0; j < file_size_blocks; j++){
        if(write(fd, buf, 1024) != 1024) {
            printf("Write error\n");
            exit(1);
        }
    }
    close(fd);
    
    // 3. Delete immediately to free space for the next iteration
    unlink(name);
  }

  end = uptime();
  printf("  -> Duration: %d ticks\n", end - start);
}

int
main(int argc, char *argv[])
{
  printf("=== INDIRECT BLOCK THRESHOLD BENCHMARK (High Volume) ===\n");
  
  // Total: 2000 Indirect Blocks allocated + 2000 Indirect Blocks freed
  bench_threshold("STANDARD", 0);
  
  // Total: 0 Indirect Blocks touched
  bench_threshold("EXTENT  ", O_EXTENT);
  
  exit(0);
}