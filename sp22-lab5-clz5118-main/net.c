#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
  int x = 0; //variable for total amount of bytes read 
  int bytesUsed = 0; //variable for bytes read after each read call 
  int position = 0; 
  while (x < len) { //go until we read lyn bytes
    bytesUsed = read(fd, &buf[position], len - position); //call read
    x += bytesUsed; //add to total 
    position += bytesUsed; //increment position 
    
  }
  if (x != len) { //check if total bytes is equal to length 
    return false;
  }
  
  
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
  int x = 0; //variable for total amoutn of bytes written 
  int bytesUsed = 0; //variable for bytes written after each write call 
  int position = 0;
  while (x < len) { //go until we write len bytes 
    bytesUsed = write(fd, &buf[position], len - position); //call write 
    x += bytesUsed; //add to total 
    position += bytesUsed; //incriment position 
    
  }
  if (x != len) { //check if total bytes is equal to length
    return false;
  }
  
  
  return true;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  uint8_t packetHeader[HEADER_LEN]; //buffer to store packet header 
  uint16_t length;
  uint16_t length2;
  uint32_t op2;
  uint32_t op3;
  uint16_t ret2;
  uint16_t ret3;
  bool nreadr;
  nreadr = nread(sd, HEADER_LEN, packetHeader); //use nread to get packet header 
  if (nreadr == true) { 
    memcpy(&length, &packetHeader[0], sizeof(length)); //find value of length stored in the jbod potocol 
    length2 = ntohs(length); //swtich length back to regular byte order 
    if (length2 == HEADER_LEN) { //if it was a write operation 
      memcpy(&op2, &packetHeader[2], sizeof(op2)); //find value of op
      memcpy(&ret2, &packetHeader[6], sizeof(ret2)); //find value of ret
      op3 = ntohl(op2); //put both in regular byte order
      ret3 = ntohs(ret2);
      *op = op3; //assign both to parameters 
      *ret = ret3;
      return true;
    }
    if (length2 == HEADER_LEN + JBOD_BLOCK_SIZE) { //if it is not a write operation 
      memcpy(&op2, &packetHeader[2], sizeof(op2)); //do same thing as above 
      memcpy(&ret2, &packetHeader[6], sizeof(ret2));
      nread(sd, JBOD_BLOCK_SIZE, block); //however, also copy packet block info into block 
      op3 = ntohl(op2);
      ret3 = ntohs(ret2);
      *op = op3;
      *ret = ret3;
      return true;
      

    }


  }
  
  return false;
}



/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint16_t length = 0;
  uint16_t length2 = 0;
  uint16_t ret = 0;
  uint16_t ret2 = 0;
  uint32_t command = op >> 26; //shift right to find command in opcode
  uint32_t op2 = 0;
  uint8_t buffer[HEADER_LEN + JBOD_BLOCK_SIZE]; //buffer to store packet plus additional block space if necessary 
  bool write;
  int position = 0; 

  
  if (command == JBOD_WRITE_BLOCK) { //if command is write block, length equals HEADER_LEN + JBOD_BLOCK_SIZE
    length = HEADER_LEN + JBOD_BLOCK_SIZE; 
  }
  else { //if not, length just equals HEADER_LEN
    length = HEADER_LEN;
  }
  length2 = htons(length); //put length, op, and ret into network byte order 
  op2 = htonl(op);
  ret2 = htons(ret);
  if (command == JBOD_WRITE_BLOCK) { //if write operation 
    memcpy(&buffer[position], &length2, sizeof(length2)); //copy data into buffer in proper order 
    position += sizeof(length2); //increment position everytime to keep track of current location in the buffer 
    memcpy(&buffer[position], &op2, sizeof(op2));
    position += sizeof(op2);
    memcpy(&buffer[position], &ret2, sizeof(ret2));
    position += sizeof(ret2);
    memcpy(&buffer[position], &block[0], JBOD_BLOCK_SIZE); //must copy block contents too because it's a write operation 
    write = nwrite(sd, HEADER_LEN + JBOD_BLOCK_SIZE, buffer); //write packet to server 
    if (write == true) { //check return value of nwrite 
      return true;
    }
    else {
      return false;
    }



    

  }
  else {
    memcpy(&buffer[position], &length2, sizeof(length2)); //copy data into buffer in proper order 
    position += sizeof(length2); //increment position everytime to keep track of current location in the buffer 
    memcpy(&buffer[position], &op2, sizeof(op2));
    position += sizeof(op2);
    memcpy(&buffer[position], &ret2, sizeof(ret2));
    position += sizeof(ret2);
    write = nwrite(sd, HEADER_LEN, buffer);
    if (write == true) {
      return true;
    }
    else {
      return false;
    }
    



  }
}



/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
  int connection;
  struct sockaddr_in caddr;

  caddr.sin_family = AF_INET; //assign caddr struct values 
  caddr.sin_port = htons(port);
  if (inet_aton(ip, &caddr.sin_addr) == 0) { //convert IPv4 address into UNIX structure 
    return false;
  }
  cli_sd = socket(AF_INET, SOCK_STREAM, 0); //assign file handle 
  if (cli_sd == -1) {
    return false;
  }
  connection = connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)); //connect to server 
  if (connection == -1) {
    return false;
  }

  return true;
}




/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd); //close the connection to the server 
  cli_sd = -1; //reset file handle 

}



/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
  bool sendpacket;
  bool recvpacket;
  uint32_t op2;
  uint16_t ret2;
  sendpacket = send_packet(cli_sd, op, block); //call send packet 
  if (sendpacket == false) {
    return -1;
  }
  recvpacket = recv_packet(cli_sd, &op2, &ret2, block); //call recieve packet 
  if (recvpacket == false) {
    return -1;
  }
  if (ret2 == -1) { //check return value 
    return -1;
  }
  return 0;

}
