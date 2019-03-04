#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <string>
#include <stdlib.h>
#include <netdb.h>
#include <poll.h>
#include "header.hpp"

// Global connection ID for just this client, as well as seq and ack
uint16_t clientID = -1;
uint32_t nextSeq = -1;
uint32_t nextACK = -1;

// Dup flag
bool duplicate = false;

// Main
int main(int argc,char* argv[]){
	if (argc != 4){
		std::cerr << "ERROR: There should be 3 arguments. USAGE: ./client <HOSTNAME/IP> <PORT> <FILENAME>\n";
		return 1;
	}

	// Create a socket using UDP IP
	int sockfd = -1;
	int port = atoi(argv[2]);
	if (port >= 0 && port <= 1023){
		std::cerr << "ERROR: Select a port outside of 0-1023\n";	
		return 1;
	}
	sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	
	
	// Convert hostname to ip
	struct hostent *host;
	if (!(host = gethostbyname(argv[1]))) {
		std::cerr << "ERROR: gethostbyname failed, incorrect hostname/port";
		return 1;
	}

	//Initialize server address
	struct sockaddr_in serveraddr;
    
	memset((char*)&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	memcpy((void *)&serveraddr.sin_addr, host->h_addr, host->h_length);


	//Init for reading file
	std::string filename = argv[3];
	FILE *fp = fopen(filename.c_str(), "r");
	if (!fp) {
		std::cerr << "ERROR: Could not open the file\n";
		return 1;
	}
	unsigned char payload[PAYLOADSIZE]; 
	memset(&payload, '\0', sizeof(payload));
	int bytesRead, bytesRecv; 

   // Create poll structure
   struct pollfd fds[1];
   fds[0].fd = sockfd;
   fds[0].events = POLLIN;
   int timemax = 500;

	//Create syn packet
	unsigned char sendSyn[PACKETSIZE] = {};
	unsigned char* syn = createSyn();
	memcpy(sendSyn,  syn, PACKETSIZE);
	
	//if timer < timeout -> check for ACK
	// if timeout -> resend SYN
	while (1){	
		if (sendto(sockfd, sendSyn, HEADERSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
			perror("sendto failed");
			return 0;
		}

		//remaddr is addr of another remote server
		struct sockaddr_in remaddr; 
		socklen_t addrlen = sizeof(remaddr);
		unsigned char buf[PACKETSIZE];
		
		//Check for SYN|ACK from server
		bytesRead = recvfrom(sockfd, buf, PACKETSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

		if (bytesRead > 0) {
			printf("received message: \"%s\"\n", buf);
			packet pack(buf, PACKETSIZE);
			
         // Store conn id into global variable
         clientID = pack.header.connID;
         nextACK = pack.header.seq + 1;
         nextSeq = pack.header.ack;

			//Create ack packet
			unsigned char sendAck[PACKETSIZE] = {};
			unsigned char* ack = createAck(pack.header.ack, pack.header.seq + 1, pack.header.connID);
			printf("RECEIVED CONNID : %u", pack.header.connID);
			memcpy(sendAck,  ack,  PACKETSIZE);
			
			//Respond with Ack
			if (sendto(sockfd, sendAck, HEADERSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
				perror("sendto failed");
				return 0;
			}
			break;
		}
	}
	
	//Begin transmission of file
	while (true) {
      // Get the file data, also check that a duplicate ack wasn't received
      if (!duplicate) {
         bytesRead = fread(payload, sizeof(char), PAYLOADSIZE, fp);
      
         // Check for erro
         if (bytesRead == 0) break;

         std::cout << "HERE IS PAYLOAD CLIENT: " << payload  << bytesRead <<  "\n";
         
         //Init new packet to send
         char sendPack[PACKETSIZE]; 
         memset(&sendPack, '\0', sizeof(sendPack));

         //Use data structure to help set up new packet
         packet pack;
         unsigned char*  hold  = pack.createPacket(payload, bytesRead, nextACK, nextSeq, clientID);
         memcpy(sendPack,  hold, PACKETSIZE);
         
         std::cout << "HERE IS SENDPACK: " << sendPack << "\n";
         std::cout << "SENDING SEQ AS : " << pack.header.seq << "\n";
         std::cout << "SENDING ACK AS : " << pack.header.ack << "\n";
         if (sendto(sockfd, sendPack, PACKETSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
            perror("sendto failed");
            return 1;
         }
      }

		//remaddr is addr of another remote server
		struct sockaddr_in remaddr; 
		socklen_t addrlen = sizeof(remaddr);

      // Now check for pollin
      int result = poll(fds, 1, timemax);
      if (result < 0) {
         std::cerr << "ERROR: unable to creating poll" << std::endl;
         exit(1);
      } else if (result > 0) {
         // Check for any sign from the server
         if (fds[0].revents & POLLIN) {
            // Create a new packet
            packet recvPacket;

            // Get the received bytes
            bytesRecv = recvfrom(sockfd, recvPacket.buf, PACKETSIZE, 0, 
                  (struct sockaddr *) &remaddr, &addrlen);

            // Check for error
            if (bytesRecv < 0) {
               std::cerr << "ERROR: recv failed" << std::endl;
               close(sockfd);
               exit(1);
            }

            // Extract relevant information from packet buffer
            packet_header recvHeader;
            memcpy(&recvHeader, (packet_header *) recvPacket.buf, 12);

            std::cout << "RECEIVED CONNID AS " << recvHeader.connID << std::endl;
            std::cout << "RECEIVED ACK AS " << recvHeader.ack << std::endl;
            std::cout << "RECEIVED SEQ AS " << recvHeader.seq << std::endl;

            // Now check that the connections are the same
            if (recvHeader.connID == clientID) {
               if (recvHeader.ack == nextSeq + bytesRead) {
                  // Set duplicate to false
                  duplicate = false;

                  // Set next seq to the ack of the server (since it should be correct)
                  nextSeq = recvHeader.ack;
                  nextACK = 0;
               } else {
                  duplicate = true;
               }
            } else {
               std::cerr << "ERROR: received wrong connection ID" << std::endl;
               return 1;
            }
         }
      } else {
         std::cerr << "ERROR: Did not receive ACK in under 0.5 seconds" << std::endl;

         // Reset pointer to beginning of file
         fseek(fp, -bytesRead, SEEK_CUR);

         // Set duplicate to false since it's resending
         duplicate = false;
      }
	}

   // At this point, send the FIN flag because it finished reading the file

	
   // Close the file descriptor
	close(sockfd);
	return 0;
}
