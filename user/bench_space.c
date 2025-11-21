#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/fs.h" // For macros

#ifndef O_EXTENT
#define O_EXTENT 0x800
#endif
#ifndef O_INLINE
#define O_INLINE 0x1000
#endif

// Extract first block number from the raw stat addresses
uint
get_start_block(char *filename)
{
  struct stat st;
  if(stat(filename, &st) < 0) return 0;
  
  // For T_FILE, addrs[0] is the first block.
  // For T_EXTENT, we must unpack the first extent.
  if(st.type == 4) { // T_EXTENT
      return (st.addrs[0] >> 8); // EXTENT_ADDR macro logic
  }
  
  // For T_INLINE, there is no block!
  if(st.type == 5) return 0; 

  return st.addrs[0];
}

// --- TEST 1: SMALL FILE EFFICIENCY ---
void
test_tiny_space(char *label, int flags)
{
  printf("Running %s (Tiny File)...\n", label);
  
  char *name = "tiny_test";
  int fd = open(name, O_CREATE | O_RDWR | flags);
  
  // Write small payload (20 bytes)
  write(fd, "12345678901234567890", 20);
  close(fd);

  struct stat st;
  stat(name, &st);

  if(st.type == 5) { // T_INLINE
      // Check if any address slots are used as block pointers
      // (They shouldn't be, they hold data)
      printf("  -> Block Address: N/A (Data in Inode)\n");
      printf("  -> Disk Blocks Consumed: 0\n");
      printf("  -> Wasted Space: 0 bytes\n");
  } else {
      printf("  -> Block Address: %d\n", st.addrs[0]);
      printf("  -> Disk Blocks Consumed: 1 (1024 bytes)\n");
      printf("  -> Wasted Space: %d bytes\n", 1024 - (int)st.size);
  }
  unlink(name);
  printf("\n");
}

// --- TEST 2: METADATA OVERHEAD GAP ANALYSIS ---
void
test_gap_analysis(char *label, int flags, int n_blocks)
{
  printf("Running %s (Gap Analysis, %d blocks)...\n", label, n_blocks);
  
  // 1. Create File A (The Subject)
  int fd_a = open("file_a", O_CREATE | O_RDWR | flags);
  char buf[1024];
  memset(buf, 'X', 1024);
  
  for(int i=0; i<n_blocks; i++) {
      if(write(fd_a, buf, 1024) != 1024) {
          printf("Write failed\n");
          exit(1);
      }
  }
  close(fd_a);

  // 2. Create File B (The Marker)
  // This file should grab the very next available block on the disk.
  int fd_b = open("file_b", O_CREATE | O_RDWR); // Standard file
  write(fd_b, "X", 1);
  close(fd_b);

  // 3. Calculate Gap
  uint start_a = get_start_block("file_a");
  uint start_b = get_start_block("file_b");
  
  // Safety check for Extents/Fragmentation
  if(start_b < start_a) {
      printf("  -> Error: Allocator wrapped around or fragmented. Can't measure gap.\n");
  } else {
      int gap = start_b - start_a;
      int overhead = gap - n_blocks;
      
      printf("  -> File A Start: %d\n", start_a);
      printf("  -> File B Start: %d\n", start_b);
      printf("  -> Total Blocks Used: %d\n", gap);
      printf("  -> Actual Data Blocks: %d\n", n_blocks);
      printf("  -> HIDDEN METADATA BLOCKS: %d\n", overhead);
      
      if(overhead == 0) 
          printf("  -> Efficiency: PERFECT (0%% Overhead)\n");
      else 
          printf("  -> Efficiency: HAS OVERHEAD (Indirect Blocks used)\n");
  }

  unlink("file_a");
  unlink("file_b");
  printf("\n");
}

int
main(int argc, char *argv[])
{
  printf("=== SPACE STORAGE EFFICIENCY TEST ===\n\n");

  // Test 1: Small Files (20 bytes)
  test_tiny_space("STANDARD", 0);
  test_tiny_space("INLINE  ", O_INLINE);

  // Test 2: Large Files (Trigger Indirect Blocks)
  // We use 260 blocks.
  // Standard Limit is 12 Direct. The rest go to Indirect.
  // This forces at least 1 Indirect Block allocation.
  int TEST_SIZE = 260; 

  test_gap_analysis("STANDARD", 0, TEST_SIZE);
  test_gap_analysis("EXTENT  ", O_EXTENT, TEST_SIZE);

  exit(0);
}