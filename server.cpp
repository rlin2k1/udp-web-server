/*
Using skeleton code from spec
*/

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

#define BUFFERSIZE 1024
#define max_clients 20

//Global variables to close gracefully during signal
std::thread threads[max_clients];
int num_conn = 1;
	
//If SIGQUIT or SIGTERM, wait for processes to finish and return 0
void signalHandler(int sigNum){
	if(sigNum == SIGPIPE || sigNum == SIGTERM){
		int j;
		for (j = 0; j < num_conn - 1; j++){
			threads[j].join();
		}
	}
}

// Saves file from client socket to specified directory
int download(int clientSockfd, std::string filepath){
	int fd = open(filepath.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd == -1){
       std::cerr << "ERROR: Failed to open file\n";
       return 1;
    }
	char buf[BUFFERSIZE] = { 0 };
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
				bytesRead = recv(clientSockfd, buf, BUFFERSIZE, 0);
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
			char buf2[BUFFERSIZE] = "ERROR";
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
		
	char buf[BUFFERSIZE] = { 0 };
	int bytesRead = -1;
	printf("Listening on localhost at port: %d\n", port);
	while(1) {
		memset(buf, '\0', sizeof(buf));	
		bytesRead= recvfrom(sockfd, buf, BUFFERSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
		if (bytesRead > 0) {
			printf("received message: \"%s\"\n", buf);
		}
	}
	
	return 0;
}