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
#include <ctime>
#include <cstdlib>
#include "header.hpp"

uint16_t clientID = -1;
uint32_t nextSeq = -1;
uint32_t nextAck = -1;
bool duplicate = false;

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

   // Buffer to read file data into
	unsigned char payload[PAYLOADSIZE]; 
	memset(&payload, '\0', sizeof(payload));
	int bytesRead, bytesRecv; 

   // Create poll structure for timer
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
	while (1) {	
      std::cout << "SEND " << 12345 << " " << 0 << " " << 0 << " <CWND> <SS-THRESH> SYN" << std::endl;
      if (sendto(sockfd, sendSyn, HEADERSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
         perror("sendto failed");
         return 0;
      }
      int result = poll(fds, 1, 10000);
      if (result < 0) {
         std::cerr << "ERROR: unable to create poll" << std::endl;
         return 1;
      } else if (result > 0) {
         //remaddr is addr of another remote server
         struct sockaddr_in remaddr;
         socklen_t addrlen = sizeof(remaddr);
         unsigned char buf[PACKETSIZE];

         //Check for SYN|ACK from server
         bytesRead = recvfrom(sockfd, buf, PACKETSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

         if (bytesRead > 0) {
            packet pack(buf, PACKETSIZE);
            std::cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " <CWND> <SS-THRESH> ACK SYN" << std::endl;

            // Store conn id into global variable
            clientID = pack.header.connID;
            nextAck = pack.header.seq + 1;
            nextSeq = pack.header.ack;

            //Create ack packet
            unsigned char sendAck[PACKETSIZE] = {};
            unsigned char* ack = createAck(pack.header.ack, pack.header.seq + 1, pack.header.connID);
            printf("RECEIVED CONNID : %u\n", pack.header.connID);
            memcpy(sendAck,  ack,  PACKETSIZE);

            std::cout << "SEND " << pack.header.ack << " " << pack.header.seq + 1 << " " << pack.header.connID << " <CWND> <SS-THRESH> ACK" << std::endl;
            //Respond with Ack
            if (sendto(sockfd, sendAck, HEADERSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
               perror("sendto failed");
               return 0;
            }
            break;
         }
      } else {
         std::cerr << "ERROR: incorrect host name (probably)" << std::endl;
         return 1;
      }
	}
	
	//Respond with ACK 
	//Begin transmission of file
	while (1) {
      // Check for duplicates
      if (!duplicate) {
         bytesRead = fread(payload, sizeof(char), PAYLOADSIZE, fp);

         // Check for EOF
         if (bytesRead == 0) break;

         //std::cout << "HERE IS PAYLOAD CLIENT: " << payload <<  "\n";
         //std::cout << "BYTES READ: " << bytesRead << std::endl;
         
         //Init new packet to send
         char sendPack[PACKETSIZE]; 
         memset(&sendPack, '\0', sizeof(sendPack));

         //Use data structure to help set up new packet
         packet pack;
         pack.setSeq(nextSeq);
         pack.setAck(nextAck);
         unsigned char*  hold  = createDataPacket(nextSeq, nextAck, clientID, payload, bytesRead);
         memcpy(sendPack,  hold, PACKETSIZE);
         
         //std::cout << "HERE IS SENDPACK: " << sendPack << "\n";
         std::cout << "SENDING SEQ AS : " << pack.header.seq << "\n";
         std::cout << "SENDING ACK AS : " << pack.header.ack << "\n";
         std::cout << "SEND " << nextSeq << " " << nextAck << " " << clientID << " <CWND> <SS-THRESH> ACK" << std::endl;
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
         std::cerr << "Error creating poll" << std::endl;
         exit(1);
      } else if (result > 0) {
         // Check for any sign from the server
         if (fds[0].revents & POLLIN) {
            // Create a new packet
            unsigned char recvBuf[PACKETSIZE];

            // Get the received bytes
            bytesRecv = recvfrom(sockfd, recvBuf, PACKETSIZE, 0, (struct sockaddr *) &remaddr, &addrlen);

            // Check for error
            if (bytesRecv < 0) {
               std::cerr << "ERROR: recv failed" << std::endl;
               close(sockfd);
               exit(1);
            }

            // Create packet from buf
            packet recvPack(recvBuf, PACKETSIZE);

            std::cout << "RECV " << recvPack.header.seq << " " << recvPack.header.ack << " " << recvPack.header.connID << " <CWND> <SS-THRESH> ACK" << std::endl;

            std::cout << "RECEIVED CONNID AS " << recvPack.header.connID << std::endl;
            std::cout << "RECEIVED ACK AS " << recvPack.header.ack << std::endl;
            std::cout << "RECEIVED SEQ AS " << recvPack.header.seq << std::endl;
            std::cout << "NEXT SEQ: " << nextSeq << std::endl;
            std::cout << "NEXT ACK: " << nextAck << std::endl;
            std::cout << "RECEIVED BYTES: " << bytesRecv << std::endl;

            // TODO: check the ACK number and make sure that it matches what we want
            // Now check that the connections are the same
            if (recvPack.header.connID == clientID) {
               if (recvPack.header.ack == nextSeq + bytesRead) {
                  std::cerr << "RECEIVED NEXT ACK" << std::endl;
                  // Set duplicate to false
                  duplicate = false;

                  // Set next seq to the ack of the server (since it should be correct)
                  nextSeq = recvPack.header.ack;
                  nextAck = 0;
               } else {
                  std::cerr << "RECEIVED DUPLICATE ACK" << std::endl;
                  std::cout << "DROP " << recvPack.header.seq << " " << recvPack.header.ack << " " << recvPack.header.connID << " <CWND> <SS-THRESH> ACK" << std::endl;
                  duplicate = true;
               }
            } else {
               std::cerr << "ERROR: received wrong connection ID: " << recvPack.header.connID << std::endl;
               return 1;
            }
         }
      } else {
         std::cerr << "ERROR: Did not receive ACK in under 0.5 seconds" << std::endl;

         // Resend packet
         fseek(fp, -bytesRead, SEEK_CUR);

         // Reset duplicate
         duplicate = false;
      }
	}
	
   // Now is time to send FIN
   unsigned char sendFin[PACKETSIZE] = {};
   unsigned char *fin = createFin(nextSeq, clientID);
   memcpy(sendFin, fin, PACKETSIZE);
   while (1) {
      std::cerr << "SENDING FIN" << std::endl;
      std::cout << "SEND " << nextSeq << " " << 0 << " " << clientID << " <CWND> <SS-THRESH> FIN" << std::endl;
      if (sendto(sockfd, sendFin, HEADERSIZE, 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
         std::cerr << "ERROR: unable to send fin packet" << std::endl;
         return 1;
      }

      // Wait for 10 seconds
      int result = poll(fds, 1, 10000);
      if (result < 0) {
         std::cerr << "ERROR: unable to create poll" << std::endl;
         return 1;
      } else if (result > 0) {
         //remaddr is addr of another remote server
         struct sockaddr_in remaddr;
         socklen_t addrlen = sizeof(remaddr);
         unsigned char buf[PACKETSIZE];

         //Check for ACK from server
         bytesRead = recvfrom(sockfd, buf, PACKETSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

         // Got information
         if (bytesRead > 0) {
            printf("received message: \"%s\"\n", buf);
            packet pack(buf, PACKETSIZE);
            if (pack.getAckFlag()) {
               std::cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " <CWND> <SS-THRESH> ACK" << std::endl;
               std::cerr << "GOT ACK BACK ______" << std::endl;
               std::cerr << "RECEIVED SEQ AS " << pack.header.seq << std::endl;
               std::cerr << "RECEIVED ACK AS " << pack.header.ack << std::endl;

               // Create remaddr
               struct sockaddr_in remaddr; 
               socklen_t addrlen = sizeof(remaddr);

               // Start timer
               clock_t startTime = clock();
               double secondsPassed;
               double secondsToDelay = 2.0;
               std::cerr << "TIME TO DELAY: " << secondsToDelay << std::endl;
               std::cerr << "STARTIME: " << startTime << std::endl;
               bool flag = true;
               while (flag) {
                  // Check that two seconds haven't passed
                  secondsPassed = (clock() - startTime) / CLOCKS_PER_SEC;
                  if (secondsPassed >= secondsToDelay) {
                     std::cerr << secondsPassed << " seconds have passed" << std::endl;
                     flag = false;
                  }
                  // Create a new packet
                  unsigned char recvBuf[PACKETSIZE];

                  // Recv from the server
                  bytesRecv = recvfrom(sockfd, recvBuf, PACKETSIZE, 0, (struct sockaddr *) &remaddr, &addrlen);

                  // Check for no data
                  if (bytesRecv > 0) {
                     // Check for FIN from server
                     packet pack(recvBuf, PACKETSIZE);
                     if (pack.getFinFlag()) {
                        std::cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " <CWND> <SS-THRESH> FIN" << std::endl;
                        // Respond with ACK
                        unsigned char sendAck[PACKETSIZE] = {};
                        unsigned char* ack = createAck(nextSeq + 1, pack.header.seq + 1, clientID);
                        printf("RECEIVED CONNID IN FIN : %u\n", pack.header.connID);
                        memcpy(sendAck,  ack,  PACKETSIZE);

                        //Respond with Ack
                        std::cout << "SEND " << nextSeq + 1 << " " << pack.header.seq + 1 << " " << clientID << " <CWND> <SS-THRESH> ACK" << std::endl;
                        if (sendto(sockfd, sendAck, HEADERSIZE, 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
                           perror("ERROR: sendto failed");
                           return 0;
                        }
                     }
                  }
               }
               break;
            }
         }
      } else {
         std::cerr << "ERROR: waited for more than 10 seconds, retrying" << std::endl;
      }
   }

   // Close the file descriptor
	close(sockfd);
	return 0;
}
