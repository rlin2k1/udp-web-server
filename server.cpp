#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <stdlib.h>

#include <thread>
#include <fcntl.h>  
#include <fstream>
#include <signal.h>
#include <sys/stat.h>

#include "header.hpp" 

#define max_clients 21

//Global variables to close gracefully during signal
std::thread threads[max_clients];
int num_conn = 1;
//Conn_state[i][0] -> holds seq # of the ith client
// Conn_state[i][1] -> holds ack# 
int conn_state[max_clients] ;
FILE *files[max_clients];

//If SIGQUIT or SIGTERM, wait for processes to finish and return 0
void signalHandler(int sigNum){
	if(sigNum == SIGPIPE || sigNum == SIGTERM){
		int j;
		for (j = 0; j < num_conn - 1; j++){
			threads[j].join();
		}
	}
   fclose(files[1]);
}

// Saves file from client socket to specified directory
int download(int clientSockfd, std::string filepath){
	int fd = open(filepath.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd == -1){
       std::cerr << "ERROR: Failed to open file\n";
       return 1;
    }
	char buf[PACKETSIZE] = { 0 };
	int res = 0;
	int bytesRead = -1;
	fd_set readfds2;
	struct timeval timer = {15, 0};	

	while(1){		;
		FD_ZERO(&readfds2);
		FD_SET(clientSockfd, &readfds2);
		memset(buf, '\0', sizeof(buf));		
		res = select( clientSockfd + 1 , &readfds2 , NULL , NULL , &timer);   
		if (res < 0){   
			std::cerr << "ERROR: select failed\n"; 
		}
		// If there was any res, read from the buffer
		else if (res > 0){
			if (FD_ISSET(clientSockfd, &readfds2)) {
				bytesRead = recv(clientSockfd, buf, PACKETSIZE, 0);
				if (bytesRead < 0) {
					std::cerr << "ERROR: recv failed";
					close(fd);
					return 1;
				}
				else if ( bytesRead > 0 ){
					if (write(fd, &buf[0], bytesRead) < 0) {
						std::cerr << "Failed to write to log file\n";
						close(fd);
						return 1;
					}
				}
				else{
					// Finished reading
					//std::cerr << "Connection has been gracefully closed\n";
					break;
				}
			}
		}
		else{
			// Timed out, leave only "ERROR" in file
			std::cerr << "ERROR: Waited over 15 seconds, Connection timed out\n";
			close(fd);
			//Reopen file with truncate to leave only "ERROR" string
			fd = open(filepath.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
			if (fd == -1){
			   std::cerr << "ERROR: Failed to open file\n";
			   return 1;
			}
			char buf2[PACKETSIZE] = "ERROR";
			if (write(fd, &buf2[0], 5) < 0) {
				std::cerr << "Failed to write to log file2\n";
				close(fd);
				return 1;
			}
			return 1;
		}
	}
	
	close(fd);
	return 0;
}

int main(int argc,char* argv[]){
	if (argc != 3){
		std::cerr << "ERROR: There should be 2 arguments. USAGE: ./server <PORT> <FILE-DIR>\n";
		return 1;
	}
	
	//Set up SIGQUIT handler
	if(signal(SIGQUIT,signalHandler) == SIG_ERR){
		std::cerr << "Error: Problem setting up SIGQUIT handler with signal()\n";
		return 1;
	}
	
	//Set up SIGTERM handler
	if(signal(SIGTERM,signalHandler) == SIG_ERR){
		std::cerr << "Error: Problem setting up SIGTERM handler with signal()\n";
		return 1;
	}
	
	// Extract port
	int port = atoi(argv[1]);
	if (port >= 0 && port <= 1023){
		std::cerr << "ERROR: Select a port outside of 0-1023\n";
		return 1;
	}
	
	// Extract file-dir
	std::string dir = argv[2];
	
	// Create dir if necessary
	mkdir(argv[2], 0777);
	
	// Create a parent socket using UDP IP
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0){
		std::cerr << "ERROR: Failed to create socket\n";
		return 1;
	}
	
	// allow others to reuse the address
	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		std::cerr << "ERROR: setsockopt\n";
		return 1;
	}

	//Set nonblocking socket
	int resp = fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
	if (resp != 0){
		std::cerr << "ERROR: fnctl failed\n";
		return 1;
	}
	
	// bind address to socket
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		std::cerr << "ERROR: bind failed, invalid port number";
		return 2;
	}
	
	//remaddr is addr of another remote server
	struct sockaddr_in remaddr; 
	socklen_t addrlen = sizeof(remaddr);
		
	unsigned char buf[PACKETSIZE] = { 0 };
	int bytesRead = -1;
	printf("Listening on localhost at port: %d\n", port);
	while (1) {
		memset(buf, '\0', sizeof(buf));
		bytesRead= recvfrom(sockfd, buf, PACKETSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
      //sleep(5);
		if (bytesRead > 0) {
         std::cerr << "BYTESREAD: " << bytesRead << std::endl;
			//printf("received message: \"%s\"\nsize=%d\n", buf, bytesRead);
			packet pack(buf, PACKETSIZE);
         std::cerr << "SIZE OF: " << sizeof(pack) << std::endl;
			printf("\nRECEIVED SEQ:%u, and ACK:%u\n\n", pack.header.seq, pack.header.ack);
			//Create new connection
			if (pack.getSynFlag()){
            std::cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " SYN" << std::endl;
            std::cerr << "RECEIVED SYN PACKET" << std::endl;
				//save state
				conn_state[num_conn] = pack.header.seq + 1;
            std::cerr << "CONN_STATE[NUM_CONN]: " << conn_state[num_conn] << std::endl;
            std::cerr << "RECEIVED SYN SEQ: " << pack.header.seq << std::endl;
            std::cerr << "RECEIVED SYN ACK: " << pack.header.ack << std::endl;
				
				//Respond:
				//Create ack packet
				unsigned char sendSynAck[PACKETSIZE] = {};
				unsigned char* synAck = createSynAck((uint16_t)num_conn);
				memcpy(sendSynAck,  synAck,  PACKETSIZE);
				
				//send out SynAck
            std::cout << "SEND " << 4321 << " " << 12346 << " " << num_conn << " ACK SYN" << std::endl;
				if (sendto(sockfd, sendSynAck, HEADERSIZE, 0, (struct sockaddr *)&remaddr, addrlen) < 0) {
					perror("sendto failed");
					return 0;
				}

            // Create file handler
            std::string filename = std::to_string(num_conn) + ".file";
            files[num_conn] = fopen(filename.c_str(), "w");
            if (!files[num_conn]) {
               std::cerr << "ERROR: could not open file" << std::endl;
               return 1;
            }

            // Increase number of connections
				num_conn++;
			} else if (pack.getFinFlag()) {
            std::cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " FIN" << std::endl;
            std::cerr << "GOT FIN FLAG" << std::endl;
            // Create ACK packet to send back
            unsigned char sendAck[PACKETSIZE] = {};
            unsigned char *ack = createAck(4322, pack.header.seq + 1, pack.header.connID);
            memcpy(sendAck, ack, PACKETSIZE);

            // Respond with ACK
            std::cout << "SEND " << 4322 << " " << pack.header.seq + 1 << " " << pack.header.connID << " ACK" << std::endl;
            if (sendto(sockfd, sendAck, HEADERSIZE, 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
               perror("ERROR: fin ack failed");
               return 1;
            }

            // Close FD
            int conn = (int) pack.header.connID;
            fclose(files[conn]);

            // ALso create FIN?
            unsigned char sendFin[PACKETSIZE] = {};
            unsigned char *fin = createFin(4322, pack.header.connID);
            memcpy(sendFin, fin, PACKETSIZE);
            std::cout << "SEND " << 4322 << " " << 0 << " " << pack.header.connID << " FIN" << std::endl;
            if (sendto(sockfd, sendFin, HEADERSIZE, 0, (struct sockaddr *) &remaddr, sizeof(remaddr)) < 0) {
               std::cerr << "ERROR: unable to send fin packet" << std::endl;
               return 1;
            }
         } else { //Handle by saving into file
            if (!pack.getAckFlag()) {
               std::cerr << "RECEIVED NORMAL HEADER" << std::endl;

               // CWIND STUFF
               int conn = (int) pack.header.connID;
               std::cerr << "EXPECTED CONN_STATE: " << conn_state[conn] << std::endl;
               std::cerr << "RECEIVED SEQ: " << pack.header.seq << std::endl;
               if (conn_state[conn] == (int) pack.header.seq) {
                  std::cerr << "STEP 2" << std::endl;
                  //do stuff as below 
                  // Try to copy into char buffer
                  char test[PAYLOADSIZE];
                  memset(&test, '\0', sizeof(test));
                  memcpy(test, buf + 12, PAYLOADSIZE);

                  //printf("received message: \"%s\"\nsize=%d\n", buf + 12, bytesRead);
                  //std::cerr << "RECEIVED MESSAGE: " << test << std::endl;
                  std::cerr << "RECEIVED CONN ID: " << pack.header.connID << std::endl;
                  std::cerr << "RECEIVED SEQ: " << pack.header.seq << std::endl;
                  
                  // Store into file
                  std::cerr << "SIZEOF RETURNS: " << sizeof(pack) << std::endl;
                  int conn = (int) pack.header.connID;
                  int fileBytesWritten = fwrite(test, sizeof(char), bytesRead - 12, files[conn]);
                  std::cerr << "BYTES WRITTEN: " << fileBytesWritten << std::endl;
                  std::cerr << "PAYLOAD SIZE: " << strlen(test) << std::endl;

                  // Send back appropriate ack
                  unsigned char sendAck[PACKETSIZE] = {};
                  unsigned char *ack =
                     createAck(pack.header.ack, pack.header.seq + fileBytesWritten, pack.header.connID);
                  memcpy(sendAck, ack, PACKETSIZE);

                  // Send ack to client
                  std::cout << "SEND " << pack.header.ack << " " << pack.header.seq + fileBytesWritten << " " << pack.header.connID << " ACK" << std::endl;
                  if (sendto(sockfd, sendAck, HEADERSIZE, 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
                     perror("ERROR: sendto failed");
                     return 1;
                  }

                  // update conn_state
                  conn_state[conn] = (int) pack.header.seq + fileBytesWritten;
               }
            } else {
               std::cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " ACK" << std::endl;
            }
			}
		}
	}
	
	return 0;
}
