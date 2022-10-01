#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cacheopen = -1; //variable for if cache is available or not 

int cache_create(int num_entries) {
  int i = 0;
  if (num_entries < 2 || num_entries > 4096) { //check to see if we're making the cache the right size 
    return -1;
  }
  if (cacheopen == 0) { //check to see if cache was already open
    return -1;
  }
  cacheopen = 0; //set cache to open
  cache = malloc(num_entries * sizeof(cache_entry_t)); //create cache
  cache_size = num_entries; //set cache size
  for (i = 0; i < cache_size; i++) { //make all cache elements false to begin with, false = not used 
    cache[i].valid = false;
  }
  

  return 1;
}

int cache_destroy(void) {
  if (cacheopen == -1) { //check to see if cache was already closed
    return -1;
  }
  cacheopen = -1; //set cache to closed
  free(cache); //free pointers
  cache = NULL;
  cache_size = 0; //set cache size to 0

  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  int i = 0;
  num_queries++; //increment number of queries 
  if (cacheopen == -1) { //checks to see if cache is closed and if we're looking up valid disks and blocks
    return -1;
  }
  if (disk_num > 15 || disk_num < 0) {
    return -1;
  }
  if (block_num > 255 || block_num < 0) {
    return -1;
  }
  for (i = 0; i < cache_size; i++) { //loop through cache
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num) { //search for matching disk and block 
      if (buf == NULL) { //fail if buf = NULL
        return -1;
      }
      else {
        if (cache[i].valid == true) { //see if cache element is actually being used
          memcpy(buf, cache[i].block, 256); //copy the data into buf
          num_hits++; //increment hits and clock, set cache element's access time equal to clock
          clock++;
          cache[i].access_time = clock; 
          return 1;

        }
        else {
          return -1;
        }
        


      }
      
    }

  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  int i = 0;
  for (i = 0; i < cache_size; i++) { //loop through cache
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid == true) { //check if disk and block match and check if element is currently in use 
      memcpy(cache[i].block, buf, 256); //coppy info into the cache element 
      clock++; //increment clock and set access time
      cache[i].access_time = clock;
    }
  }


}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  int i = 0;
  int nextEmpty = 0; //next empty position in cache
  int isFull = 0; //flag for if the cache is full 
  int minimum = 0; //minumum checker for LRU entry 
  int LRUentry = 0; //least recently used cache element
  if (cacheopen == -1) { //checks if cache is closed, buf == NULL, and if the disk and block are valid 
    return -1;
  }
  if (buf == NULL) {
    return -1;
  }
  if (disk_num > 15 || disk_num < 0) {
    return -1;
  }
  if (block_num > 255 || block_num < 0) {
    return -1;
  }
  for (i = 0; i < cache_size; i++) { //if entry already exists, fail 
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid == true) {
      return -1;
    }
  }
  for (i = 0; i < cache_size; i++) { //finds next empty spot in cache 
    if (cache[i].valid == false) { //makes sure spot isn't currently in use 
      nextEmpty = i;
      break;
    }
    if (i == cache_size - 1) { //if we reached the end of the for loop and there aren't any empty spots, set isFull to 1
      isFull = 1;
    }
  }
  if (isFull != 1) { //if cache isn't full 
    cache[nextEmpty].disk_num = disk_num; //set cache element to all the corresponding information
    cache[nextEmpty].block_num = block_num;
    memcpy(cache[nextEmpty].block, buf, 256);
    cache[nextEmpty].valid = true;
    clock++;
    cache[nextEmpty].access_time = clock;
  }
  else {
    minimum = cache[0].access_time; //sets miniumum to first cache element initially 
    for (i = 0; i < cache_size; i++) { //loops through cache checking access time to find the smallest access time 
      if (cache[i].access_time < minimum) {
        minimum = cache[i].access_time;
        LRUentry = i; //sets LRUentry to the element with smallest access time 
      }
    }
    cache[LRUentry].disk_num = disk_num; //set cache element to all the corresponding information 
    cache[LRUentry].block_num = block_num;
    memcpy(cache[LRUentry].block, buf, 256);
    cache[LRUentry].valid = true;
    clock++;
    cache[LRUentry].access_time = clock;
  }
  
    

  return 1;
}

bool cache_enabled(void) {
  if (cache_size >= 2) { //checks if cache_size is greater than or equal to 2 
    return true;
  }
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}