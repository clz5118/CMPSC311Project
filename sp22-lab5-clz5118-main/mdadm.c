#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include "cache.h"

#include "mdadm.h"
#include "jbod.h"
#include "net.h"

int mount = -1; //global variable for mount state, -1 for unmounted 0 for mounted
int blockPosition = 0; //global variable for block number
int diskPosition = 0; //global variable for disk number
int location = 0; //global variable for offset within block 



uint32_t construction(uint32_t command, uint32_t diskID, uint32_t reserved, uint32_t blockID) { //op construction function 
  uint32_t op = 0;
  uint32_t tempa = 0; 
  uint32_t tempb = 0;
  uint32_t tempc = 0;
  uint32_t tempd = 0;
  tempa = (command & 0x3f) << 26; //ensure we're getting only bottom 6 bits, shift to position
  tempb = (diskID & 0xf) << 22; //ensure we're getting only bottom 4 bits, shift to position 
  tempc = (reserved & 0x3fff) << 8; //ensure we're getting only bottom 14 bits, shift to position
  tempd = blockID & 0xff; //ensure we're getting only bottom 8 bits, no need for shift
  op = tempa|tempb|tempc|tempd; //or pieces together to create op
  return op;
}

int mdadm_mount(void) {
  uint32_t op = construction(JBOD_MOUNT, 0, 0, 0); //create mount op
  int opResult = jbod_client_operation(op, NULL); //call jbod to mount 
  if (mount == 0) { //if mount is already mounted, can't mount again
    return -1;
  }
  else {
    if (opResult == 0) { //if jbod returned success, mount
      mount = 0;
    }
    else {
      return -1; 
    }
  }
  return 1;
}

int mdadm_unmount(void) {
  uint32_t op = construction(JBOD_UNMOUNT, 0, 0, 0); //create unmount op
  int opResult = jbod_client_operation(op, NULL); //call jbod to unmount
  if (mount == -1) { //if alreayd unmounted, can't unmount again
    return -1;
  }
  else {
    if (opResult == 0) { //if jbod returned success, unmount
      mount = -1;
    }
    else {
      return -1;
    }
  }
  return 1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  
  uint32_t op = 0;
  int opResult = 0;
  uint8_t buffer[JBOD_BLOCK_SIZE]; //my own buffer
  bool cacheenabled;
  int cachelookup = 0;
  int cacheinsert = 0;

  if (mount == -1) { //check if mounted already 
    return -1;
  }
  if (len > 1024) { //check to make sure len < 1024
    return -1;
  }
  if (addr < 0 || addr + len > 1048576) { //making sure we're reading within bounds 
    return -1;
  }
  if (buf == NULL && len > 0) { //buf can't be NULL if we're reading something 
    return -1;
  }
  

  int bytesUsed = 0; //how many bytes have currently been read
  int bytesUnused = len; //how many bytes are currently left to be read 

  mdadm_seek(addr); //initial seek to current address
  while (bytesUnused > 0) { //ensures that we are only reading if there are bytes left to be read
    cacheenabled = cache_enabled(); //check to make sure cache is enabled
    if (cacheenabled == true) {
      mdadm_seek(addr + bytesUsed);
      cachelookup = cache_lookup(diskPosition, blockPosition, buffer); //check if block is in cache
      
      if (cachelookup == -1) { //if block not in cache
        
        op = construction(JBOD_READ_BLOCK, 0, 0, 0); //perform read operation to get block
        opResult = jbod_client_operation(op, buffer);
        blockPosition += 1; //increment block position
        cacheinsert = cache_insert(diskPosition, blockPosition - 1, buffer); //insert the block into the cache
        if (cacheinsert == -1) { //check to see if block couldn't be inserted
          return -1;
        }
      }
      
    }
    else {
      op = construction(JBOD_READ_BLOCK, 0, 0, 0);
      opResult = jbod_client_operation(op, buffer);
      blockPosition += 1;
    } 
    if (bytesUnused > 256 - location) { //if reading across blocks 
      if (opResult == 0) { //if read operation returned success 
        memcpy(&buf[bytesUsed], &buffer[location], 256 - location); //copy amount of bytes to end of current block from my buffer to buf 
        bytesUsed += 256 - location; //increment bytesUsed, bytesUnused, and location by amount read 
        bytesUnused -= 256 - location;
        location += 256 - location;
      }
      else {
        return -1;
      }
      
    }
    else { //if not reading across blocks 
      if (opResult == 0) { //if read operation returned success 
        memcpy(&buf[bytesUsed], &buffer[location], bytesUnused); //copy remaining bytes from my buffer to buf 
        bytesUsed += bytesUnused; //increment bytesUsed, bytesUnused, and location by amount read 
        bytesUnused -= bytesUnused;
        location += bytesUnused;
      }
      else {
        return -1;
      }
    }
    if (blockPosition > 255) { //if reading across disks 
      mdadm_seek(addr + bytesUsed); //seek to first byte of the next disk 
      
    }
    if (location > 255) { //ensures location stays between 0 and 255 as that is what we're using for calcuations 
      location = location % 256; //if location above 255, turn it into one that is between 0 and 255 
    }
    
  
  }


  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {

  uint32_t op = 0;
  int opResult = 0;
  uint8_t buffer[JBOD_BLOCK_SIZE];
  bool cacheenabled;
  int cachelookup = 0;
  int cacheinsert = 0;
  

  if (mount == -1) { //check if mounted already 
    return -1;
  }
  if (len > 1024) { //check to make sure len < 1024
    return -1;
  }
  if (addr < 0 || addr + len > 1048576) { //making sure we're writing within bounds 
    return -1;
  }
  if (buf == NULL && len > 0) { //buf can't be NULL if we're writing something 
    return -1;
  }
  

  int bytesUsed = 0; //how many bytes have currently been written
  int bytesUnused = len; //how many bytes are currently left to be written 
  
  mdadm_seek(addr); //inital seek to address 
  
  while (bytesUnused > 0) { //while we still have bytes left to be written 
    cacheenabled = cache_enabled(); //check if cache is enabled
    if (cacheenabled == true) {
      mdadm_seek(addr + bytesUsed);
      cachelookup = cache_lookup(diskPosition, blockPosition, buffer); //see if cache block is in cache
      if (cachelookup == -1) { //if not in cache already
        
        op = construction(JBOD_READ_BLOCK, 0, 0, 0); //construct read operation
        opResult = jbod_client_operation(op, buffer);
        blockPosition += 1; //increment block position 
        cacheinsert = cache_insert(diskPosition, blockPosition - 1, buffer); //insert block into cache
        if (cacheinsert == -1) { //check to see if block couldn't be inserted
          return -1;
        }
        
      }
    }
    else {
      op = construction(JBOD_READ_BLOCK, 0, 0, 0);
      opResult = jbod_client_operation(op, buffer);
      blockPosition += 1; //increment block position 
      
    } 
     
    if (bytesUnused > 256 - location) { //if writing across blocks 
      if (opResult == 0) { //if operation returned success 
        
        memcpy(&buffer[location], &buf[bytesUsed], 256 - location); //copy amount of bytes to end block from buf to buffer 
        mdadm_seek(addr + bytesUsed); //reseek to the block we need to write to 
        op = construction(JBOD_WRITE_BLOCK, 0, 0, 0);
        opResult = jbod_client_operation(op, buffer); //calling write operation 
        if (cacheenabled == true) {
          cache_update(diskPosition, blockPosition, buffer);

        }
        
        if (opResult == 0) { //if operation returned success 
          blockPosition += 1; //increment block position, bytesUsed, bytesUnused, and location
          bytesUsed += 256 - location;
          bytesUnused -= 256 - location;
          location += 256 - location;
        }
        else {
          return -1;
        }
        
        
      }
      else {
        return -1;
      }
      
    }
    else { //if not writing across blocks 
      if (opResult == 0) {
        memcpy(&buffer[location], &buf[bytesUsed], bytesUnused); //copy remaining bytes from buf to buffer 
        mdadm_seek(addr + bytesUsed); //reseek to the block we need to write to 
        op = construction(JBOD_WRITE_BLOCK, 0, 0, 0);
        opResult = jbod_client_operation(op, buffer); //calling write operation 
        if (cacheenabled == true) {
          cache_update(diskPosition, blockPosition, buffer);

        }
        if (opResult == 0) { //if operation returned success 
          blockPosition += 1; //increment block position, bytesUsed, bytesUnused, and location 
          location += bytesUnused;
          bytesUsed += bytesUnused;
          bytesUnused -= bytesUnused;
          
        }
        else {
          return -1;
        }
        
      }
      else {
        return -1;
      }
    }
    if (blockPosition > 255) { //if writing across disks 
      mdadm_seek(addr + bytesUsed); //seek to first position of next disk 
      
      
    }
    if (location > 255) { //ensures location stays between 0 and 255 
      location = location % 256;
    }
    
  
  }


  return len;
}

void mdadm_seek(uint32_t addr) {
  uint32_t disk = 0; //variable for disk position 
  uint32_t block = 0; //variable for block position 
  uint32_t op = 0;
  uint32_t op2 = 0;
  int opResult = 0;
  int opResult2 = 0; 
  disk = addr / JBOD_DISK_SIZE; //disk number calculation 
  block = addr % JBOD_DISK_SIZE; 
  block /= JBOD_BLOCK_SIZE; //block number calculation 
  
  op = construction(JBOD_SEEK_TO_DISK, disk, 0, 0); //create op for seek to disk 
  opResult = jbod_client_operation(op, NULL); //call jbod to seek to disk 
  if (opResult == 0) { //ensure jbod success 
    op2 = construction(JBOD_SEEK_TO_BLOCK, 0, 0, block); //if disk operation was successful, try seeking to block 
    opResult2 = jbod_client_operation(op2, NULL);
    if (opResult2 == 0 ) { //if block operation also successful 
      blockPosition = block; //set block, disk, and offset location to global variables 
      diskPosition = disk;
      location = addr % JBOD_BLOCK_SIZE;
    }
  }
}