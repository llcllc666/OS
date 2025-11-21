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

void
bench_small_files(char *label, int flags)
{
  int start, end;
  int N = 500; // Number of files
  char name[16];
  char data[] = "This is a small test string."; // ~28 bytes

  printf("Running %s test (%d files)...\n", label, N);
  start = uptime();

  for(int i = 0; i < N; i++){
    // 1. Create Name
    name[0] = 'f';
    name[1] = '0' + (i % 10); // crude naming
    name[2] = 0;

    // 2. Create & Write
    int fd = open(name, O_CREATE | O_RDWR | flags);
    if(fd < 0){
        printf("Failed to create file\n");
        exit(1);
    }
    write(fd, data, sizeof(data));
    close(fd);

    // 3. Delete immediately (to prevent running out of disk space)
    unlink(name);
  }

  end = uptime();
  printf("  -> Duration: %d ticks\n", end - start);
}

void
bench_large_io(char *label, int flags)
{
  int start, end;
  int n_blocks = 200; // Keep this safe for Standard FS
  int iterations = 20; // NEW: Repeat the test 20 times to amplify difference
  char buf[1024];
  memset(buf, 'A', 1024);

  printf("Running %s test (%d blocks x %d loops)...\n", label, n_blocks, iterations);
  
  start = uptime();

  for(int k = 0; k < iterations; k++) {
      // Unique name per iteration to prevent caching artifacts
      char name[16];
      name[0] = 'b'; name[1] = 'i'; name[2] = 'g';
      name[3] = '0' + (k % 10); name[4] = 0;

      int fd = open(name, O_CREATE | O_RDWR | flags);
      if(fd < 0){
          printf("Failed to create file\n");
          exit(1);
      }

      // Write loop
      for(int i = 0; i < n_blocks; i++){
          if(write(fd, buf, 1024) != 1024){
              printf("Write failed\n");
              exit(1);
          }
      }
      close(fd);
      unlink(name); // Clean up
  }

  end = uptime();
  printf("  -> Duration: %d ticks\n", end - start);
}

int
main(int argc, char *argv[])
{
  printf("=== XV6 FILESYSTEM BENCHMARK ===\n");

  // --- TEST 1: SMALL FILES ---
  // Compare Standard (No flags) vs Inline (O_INLINE)
  bench_small_files("STANDARD (Small)", 0);
  bench_small_files("INLINE   (Small)", O_INLINE);

  printf("\n");

  // --- TEST 2: LARGE FILES ---
  // Compare Standard (No flags) vs Extent (O_EXTENT)
  bench_large_io("STANDARD (Large)", 0);
  bench_large_io("EXTENT   (Large)", O_EXTENT);

  exit(0);
}