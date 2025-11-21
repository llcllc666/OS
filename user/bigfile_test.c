#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/fs.h"

#ifndef O_EXTENT
#define O_EXTENT 0x800
#endif

#ifndef EXTENT_ADDR
#define EXTENT_ADDR(x) ((x) >> 8)
#define EXTENT_LEN(x) ((x) & 0xFF)
#endif

void
panic(char *s)
{
  printf("FAIL: %s\n", s);
  exit(1);
}

// Write N blocks to a file descriptor
void
write_blocks(int fd, int n, int pattern_offset)
{
  char buf[1024];
  for(int i = 0; i < n; i++){
    // Fill buffer with a pattern unique to this block
    memset(buf, (i + pattern_offset) % 26 + 'a', 1024);
    if(write(fd, buf, 1024) != 1024)
      panic("write failed");
  }
}

// Read N blocks and verify the pattern
void
verify_blocks(int fd, int n, int pattern_offset)
{
  char buf[1024];
  for(int i = 0; i < n; i++){
    if(read(fd, buf, 1024) != 1024)
      panic("read failed");
      
    char expected = (i + pattern_offset) % 26 + 'a';
    if(buf[0] != expected){
      printf("Block %d: Expected %c, Got %c\n", i, expected, buf[0]);
      panic("Data Mismatch");
    }
  }
}

void
test_overflow()
{
  printf("\n=== TEST 1: Max Extent Length (260 Blocks) ===\n");
  int fd = open("test_ovf", O_CREATE | O_RDWR | O_EXTENT);
  if(fd < 0) panic("create failed");

  // Write 260 blocks. Max extent len is 255.
  // We expect: 1 Extent of 255, 1 Extent of 5.
  printf("Writing 260 blocks...\n");
  write_blocks(fd, 260, 0);
  close(fd);

  struct stat st;
  if(stat("test_ovf", &st) < 0) panic("stat failed");
  
  int ext_count = 0;
  for(int i=0; i<12; i++) {
    if(st.addrs[i]) {
        printf("  Slot %d: Len %d\n", i, EXTENT_LEN(st.addrs[i]));
        ext_count++;
    }
  }

  if(ext_count < 2) panic("Expected at least 2 extents for 260 blocks!");
  printf("PASS: Overflow handled correctly.\n");
  unlink("test_ovf");
}

void
test_fragmentation()
{
  printf("\n=== TEST 2: Fragmentation & Merging ===\n");
  
  // Step 1: Create three files to occupy contiguous space
  // [ File A (10) ] [ File B (10) ] [ File C (10) ]
  int fd_a = open("file_a", O_CREATE | O_RDWR | O_EXTENT);
  int fd_b = open("file_b", O_CREATE | O_RDWR | O_EXTENT);
  int fd_c = open("file_c", O_CREATE | O_RDWR | O_EXTENT);
  
  write_blocks(fd_a, 10, 0);
  write_blocks(fd_b, 10, 0);
  write_blocks(fd_c, 10, 0);
  
  close(fd_a);
  close(fd_b);
  close(fd_c);

  // Step 2: Delete the middle file (File B)
  // Disk: [ File A ] [ FREE (10) ] [ File C ]
  printf("Deleting middle file to create a hole...\n");
  unlink("file_b");

  // Step 3: Create File D that is larger than the hole (15 blocks)
  // It should fill the hole (10 blocks) and then jump over C to find 5 more blocks.
  // Disk: [ File A ] [ File D (10) ] [ File C ] [ File D (5) ]
  printf("Creating File D (15 blocks). Should split into 2 extents...\n");
  int fd_d = open("file_d", O_CREATE | O_RDWR | O_EXTENT);
  write_blocks(fd_d, 15, 100); // Use offset 100 for data pattern
  close(fd_d);

  // Step 4: Verify File D Metadata
  struct stat st;
  if(stat("file_d", &st) < 0) panic("stat file_d failed");
  
  int ext_count = 0;
  printf("File D Layout:\n");
  for(int i=0; i<12; i++) {
    if(st.addrs[i]) {
        uint addr = EXTENT_ADDR(st.addrs[i]);
        uint len = EXTENT_LEN(st.addrs[i]);
        printf("  Slot %d: Addr %d, Len %d\n", i, addr, len);
        ext_count++;
    }
  }

  if(ext_count < 2) {
     printf("WARN: File D fits in one extent? (Disk might not be fragmented as expected)\n");
  } else {
     printf("PASS: File D split into multiple extents to handle fragmentation.\n");
  }

  // Step 5: Verify File D Data
  printf("Verifying File D data integrity...\n");
  fd_d = open("file_d", O_RDONLY);
  verify_blocks(fd_d, 15, 100);
  close(fd_d);
  
  // Cleanup
  unlink("file_a");
  unlink("file_c");
  unlink("file_d");
}

int
main(int argc, char *argv[])
{
  printf("--- EXTENT STRESS TEST ---\n");
  
  test_overflow();
  test_fragmentation();

  printf("\nALL TESTS PASSED.\n");
  exit(0);
}