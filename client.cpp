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
#include "header.hpp"


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
	int bytesRead; 

	//Create syn packet
	unsigned char sendSyn[PACKETSIZE] = {};
	unsigned char* syn = createSyn();
	memcpy(sendSyn,  syn, PACKETSIZE);
	
	//if timer < timeout -> check for ACK
	// if timeout -> resend SYN
	while(1){	
		if (sendto(sockfd, sendSyn, HEADERSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
			perror("sendto failed");
			return 0;
		}

		//remaddr is addr of another remote server
		struct sockaddr_in remaddr; 
		socklen_t addrlen = sizeof(remaddr);
		unsigned char buf[PACKETSIZE];
		
		//Check for SYN|ACK from server
		bytesRead= recvfrom(sockfd, buf, PACKETSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
		if (bytesRead > 0) {
			printf("received message: \"%s\"\n", buf);
			packet pack(buf, PACKETSIZE);
			
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
	
	//Respond with ACK 
	
	/*
	//Begin transmission of file
	while((bytesRead = fread(payload, sizeof(char), PAYLOADSIZE, fp)) > 0){
		std::cout << "HERE IS PAYLOAD CLIENT: " << payload  << bytesRead <<  "\n";
		
		//Init new packet to send
		char sendPack[PACKETSIZE]; 
		memset(&sendPack, '\0', sizeof(sendPack));
		//Use data structure to help set up new packet
		packet pack;
		unsigned char*  hold  = pack.createPacket(payload, bytesRead);
		memcpy(sendPack,  hold, PACKETSIZE);
		
		std::cout << "HERE IS SENDPACK: " << sendPack << "\n";
		std::cout << "SENDING SEQ AS : " << pack.header.seq << "\n";
		std::cout << "SENDING ACK AS : " << pack.header.ack << "\n";
		if (sendto(sockfd, sendPack, PACKETSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
			perror("sendto failed");
			return 0;
		}
	}
	*/
	close(sockfd);
	return 0;
}