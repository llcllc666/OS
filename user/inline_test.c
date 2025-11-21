#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/fs.h"

// Ensure definitions match kernel
#ifndef O_INLINE
#define O_INLINE 0x1000
#endif

#ifndef T_INLINE
#define T_INLINE 5
#endif

void
panic(char *s)
{
  printf("FAIL: %s\n", s);
  exit(1);
}

// Helper to create filenames like "if_12" without snprintf
void
fmt_name(char *buf, int id)
{
  buf[0] = 'i';
  buf[1] = 'f';
  buf[2] = '_';
  
  // Simple integer to string (supports 0-99)
  if(id < 10){
    buf[3] = id + '0';
    buf[4] = 0;
  } else {
    buf[3] = (id / 10) + '0';
    buf[4] = (id % 10) + '0';
    buf[5] = 0;
  }
}

// --- TEST 1: CAPACITY LIMITS (STRICT) ---
void
test_capacity()
{
  printf("\n=== TEST 1: Capacity Check (Strict Mode) ===\n");
  int fd = open("in_cap", O_CREATE | O_RDWR | O_INLINE);
  if(fd < 0) panic("create failed");

  // Try to write a large buffer
  char buf[512];
  memset(buf, 'X', 512);
  
  printf("Attempting to write 512 bytes to inline file...\n");
  int n = write(fd, buf, 512);
  
  // EXPECTATION: -1 (Error) because 512 > 116 (or 52)
  if(n != -1) {
    printf("Write returned: %d\n", n);
    panic("Write succeeded! Expected error (-1) due to overflow.");
  }
  
  printf("Write returned -1 (Expected).\n");
  close(fd);

  // Verify size is 0 (Atomicity check)
  // The file should be empty because the write was rejected entirely.
  struct stat st;
  stat("in_cap", &st);
  if(st.size != 0) {
      printf("Stat size: %l\n", st.size);
      panic("Stat size mismatch! File should be empty.");
  }
  
  unlink("in_cap");
  printf("PASS: Capacity strictly enforced.\n");
}

// --- TEST 2: TRUNCATION & REWRITE ---
void
test_rewrite()
{
  printf("\n=== TEST 2: Truncation & Rewrite ===\n");
  int fd = open("in_rewrite", O_CREATE | O_RDWR | O_INLINE);
  
  // 1. Write Initial Data (Small enough to fit)
  write(fd, "OldData", 7);
  close(fd);

  // 2. Open with TRUNC flag
  fd = open("in_rewrite", O_RDWR | O_TRUNC); 
  
  struct stat st;
  if(fstat(fd, &st) < 0) panic("fstat failed");
  if(st.size != 0) panic("O_TRUNC did not set size to 0");

  // 3. Write New Data
  write(fd, "New", 3);
  close(fd);

  // 4. Verify
  fd = open("in_rewrite", O_RDONLY);
  char buf[16];
  memset(buf, 0, 16);
  read(fd, buf, 16);
  
  // Use safe string compare
  if(strcmp(buf, "New") != 0) {
      printf("Got: '%s', Expected: 'New'\n", buf);
      panic("Truncation failed");
  }
  
  printf("PASS: O_TRUNC works correctly.\n");
  close(fd);
  unlink("in_rewrite");
}

// --- TEST 3: THE "TROJAN" DELETION ---
void
test_trojan()
{
  printf("\n=== TEST 3: Safety Check (The Trojan Delete) ===\n");
  
  // 1. Create a Standard File (The Victim)
  int fd_vic = open("victim", O_CREATE | O_RDWR); 
  write(fd_vic, "VictimData", 10);
  struct stat st_vic;
  fstat(fd_vic, &st_vic);
  
  // Grab the block number it used (Slot 0)
  uint victim_block = st_vic.addrs[0];
  printf("Victim file using block: %d\n", victim_block);
  close(fd_vic);

  // 2. Create an Inline File (The Trojan)
  int fd_troj = open("trojan", O_CREATE | O_RDWR | O_INLINE);
  
  // We write DATA that looks exactly like the victim's block number
  uint data_as_int = victim_block; 
  write(fd_troj, &data_as_int, sizeof(uint));
  close(fd_troj);

  // 3. Delete the Trojan
  printf("Deleting inline file containing data '%d'...\n", victim_block);
  unlink("trojan");

  // 4. Create a NEW file to see if Block was freed
  int fd_new = open("attacker", O_CREATE | O_RDWR);
  write(fd_new, "Overwrit", 8);
  struct stat st_new;
  fstat(fd_new, &st_new);
  uint new_block = st_new.addrs[0];
  printf("New file allocated block: %d\n", new_block);
  close(fd_new);

  if(new_block == victim_block) {
      // Verify if corruption happened
      fd_vic = open("victim", O_RDONLY);
      char buf[16];
      memset(buf, 0, 16);
      read(fd_vic, buf, 10);
      
      if(strcmp(buf, "VictimData") != 0) {
          panic("CRITICAL: Inline deletion corrupted standard file system!");
      }
      printf("WARN: Allocator reused block, but data survived. Risky.\n");
  }

  printf("PASS: Standard files safe from inline deletion.\n");
  unlink("victim");
  unlink("attacker");
}

// --- TEST 4: MASS CREATION ---
void
test_mass()
{
    printf("\n=== TEST 4: Mass Creation & Deletion (Stress) ===\n");
    char name[16];
    
    // Create 30 files
    for(int i=0; i<30; i++){
        fmt_name(name, i); 
        
        int fd = open(name, O_CREATE | O_RDWR | O_INLINE);
        if(fd < 0) panic("Failed to create file in loop");
        
        char c = 'A' + i;
        write(fd, &c, 1);
        close(fd);
    }
    printf("Created 30 inline files.\n");

    // Verify and Delete
    for(int i=0; i<30; i++){
        fmt_name(name, i);
        
        int fd = open(name, O_RDONLY);
        if(fd < 0) panic("Failed to open file in verify loop");
        
        char c, expected = 'A' + i;
        read(fd, &c, 1);
        close(fd);

        if(c != expected) panic("Data mismatch in stress test");
        
        if(unlink(name) < 0) panic("Failed to unlink");
    }
    printf("PASS: Mass creation/deletion successful.\n");
}

int
main(int argc, char *argv[])
{
  printf("--- INLINE FILE STRESS TEST (STRICT MODE) ---\n");
  
  test_capacity();
  test_rewrite();
  test_trojan();
  test_mass();

  printf("\nALL TESTS PASSED.\n");
  exit(0);
}