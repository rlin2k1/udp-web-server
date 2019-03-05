/* client.cpp
The client connects to the server and as soon as connection established, sends
the content of a file to the server.

Author(s):
  Roy Lin, Grand Huynh, Julien Collins

Date Created:
  March 1st, 2019
*/

// -------------------------------------------------------------------------- //
// Import Statements for the Necessary Packages
// -------------------------------------------------------------------------- //
#include <sys/types.h>
#include <sys/socket.h> //For the Socket()
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h> //String Library
#include <stdio.h> //Standard Input/Ouput Library
#include <errno.h> //Error Handling
#include <unistd.h>
#include <thread> //For Multi-Threading - Handling Multiple Clients
#include <iostream> //Input and Output
#include <fcntl.h> //For Socket Non Blocking
#include <netdb.h> //For GetHostByName()
#include "header.hpp" //TCP Packet Header

#include <stdlib.h>
#include <poll.h>
#include <ctime>
#include <cstdlib>

using namespace std; //Using the Standard Namespace

uint16_t clientID = -1;
uint32_t nextSeq = -1;
uint32_t nextAck = -1;
bool duplicate = false;

int main(int argc, char *argv[]) //Main Function w/ Arguments from Command Line
{
   // ------------------------------------------------------------------------ //
   // Error Handling from Arguments
   // ------------------------------------------------------------------------ //
   if(argc != 4) {
      cerr << "ERROR: Need 3 Arguments: HostName, Port Number, and File Name to " 
         << "Transfer" << endl;
      exit(3);
   }

   const char* hostname = argv[1]; //The HostName/IP Address is First Argument
   int port_number = -1; //Sentinel
   try{
      port_number = stoi(argv[2]); //The Port Number is the Second Argument
   }
   catch(std::invalid_argument& e) {
      cerr << "ERROR: Invalid Port. Please Enter Valid Port NUMBER" << endl;
      exit(3);
   }
   if(port_number <= 1023){
      cerr << "ERROR: Reserved Port Number Detected. Please Specify a Port Number"
         << " Greater than 1023" << endl;
      exit(3);
   }
   string file_name = argv[3]; //Assume File Name is Always Correct - 3rd Arg
   FILE *fs = fopen(file_name.c_str(), "r"); //Open the File for Reading
   if(fs == nullptr){
      cerr << "ERROR: File Does NOT Exist!" << endl;
      exit(3);
   }

   //cerr << "Hostname to Be Connected to: " << hostname << endl;
   //cerr << "Port Number to Be Connected to: " << port_number << endl;
   //cerr << "File Name to Transfer: " << file_name << endl;

   // ----------------------------------------------------------------------- //
   // Create a Socket using TCP IP
   // ----------------------------------------------------------------------- //
   int sockfd; //Socket File Descriptor
   struct addrinfo hints, *servinfo, *p; //For getaddrinfo()
   int rv; //Check Return Value of getaddrinfo()

   memset(&hints, 0, sizeof hints); //Initialize Hints
   hints.ai_family = AF_INET; // AF_INET for IPv4
   hints.ai_socktype = SOCK_DGRAM;

   if ((rv = getaddrinfo(hostname, argv[2], &hints, &servinfo)) != 0)//GetAddrInfo
   {
      cerr << "ERROR: Get Address Info Failed" << endl;
      fclose(fs);
      exit(3);
   }
      
   // Loop through all results and try to connect
   for(p = servinfo; p != NULL; p = p->ai_next) {
      //sockfd contains the file descriptor to access the Socket
      if ((sockfd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol)) == -1) 
      { //Create Socket for the Result
         cerr << "ERROR: Failed to Create Socket" << endl;
         continue; //On to the Next Result
      }
      break; // Must have connected successfully
   }

   if (p == NULL) {
      //End of List with No Successful Bind
      cerr << "ERROR: Failed to Bind Socket" << endl;
      fclose(fs);
      exit(3);
   }

   freeaddrinfo(servinfo); // Deallocate Structure

   //Convert Hostname to Ip
	struct hostent *host;
	if (!(host = gethostbyname(argv[1]))) {
		cerr << "ERROR: gethostbyname Failed, Incorrect Hostname/Port" << endl;
		return 1;
	}
	//Initialize Server Address
	struct sockaddr_in serveraddr;
	memset((char*)&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port_number);
	memcpy((void *)&serveraddr.sin_addr, host->h_addr, host->h_length);

   //Buffer to Read File Data Into
   unsigned char payload[PAYLOADSIZE];
   memset(&payload, '\0', sizeof(payload));
   int bytesRead, bytesRecv;

   // Create Poll Structure for Timer
   struct pollfd fds[1];
   fds[0].fd = sockfd;
   fds[0].events = POLLIN;
   int timemax = 500; //Half a Second

   //Create Syn Packet
   unsigned char sendSyn[PACKETSIZE] = {0};
   unsigned char* syn = createSyn();
   memcpy(sendSyn, syn, PACKETSIZE);

   // ------------------------------------------------------------------------ //
   // THREE WAY HANDSHAKE!!!
   // ------------------------------------------------------------------------ //
   cerr << "START THREE WAY HANDSHAKE-------------------------------------------" << endl;
   while (1) {	
      cout << "SEND " << 12345 << " " << 0 << " " << 0 << " <CWND> <SS-THRESH> SYN" << endl;
      //Send First HandShake to Server!
      if (sendto(sockfd, sendSyn, HEADERSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
         perror("ERROR: Sendto Failed!");
         return 0;
      }
      int result = poll(fds, 1, 15000); //Poll 15 Seconds for Response
      if (result < 0) {
         cerr << "ERROR: Unable to Create Poll" << endl;
         return 1;
      } 
      else if (result > 0) {
         struct sockaddr_in remaddr; //Store Server Address
         socklen_t addrlen = sizeof(remaddr); //Store Size of Server Address

         unsigned char buf[PACKETSIZE];
         //Check for SYN|ACK from Server
         bytesRead = recvfrom(sockfd, buf, PACKETSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

         if (bytesRead > 0) { //We Got a Packet from the Server. Second HandShake
            packet pack(buf, PACKETSIZE);
            cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " <CWND> <SS-THRESH> ACK SYN" << endl;

            // Store Connetion ID into a Global Variable
            clientID = pack.header.connID;
            nextAck = pack.header.seq + 1;
            nextSeq = pack.header.ack;

            //Create a ACKNOWLEDGEMENT Packet
            unsigned char sendAck[PACKETSIZE] = {0};
            unsigned char* ack = createAck(pack.header.ack, pack.header.seq + 1, pack.header.connID);
            memcpy(sendAck,  ack,  PACKETSIZE);

            cout << "SEND " << pack.header.ack << " " << pack.header.seq + 1 << " " << pack.header.connID << " <CWND> <SS-THRESH> ACK" << endl;
            //Respond with ACKNOWLEDGEMENT - Send it Out!
            if (sendto(sockfd, sendAck, PACKETSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
               perror("ERROR: Sendto Failed!");
               return 0;
            }
            break;
         }
      } 
      else {
         cerr << "ERROR: CONNECTION TIMED OUT!" << endl;
         return 1;
      }
   }
   cerr << "BEGIN TRANSMISSION OF FILE------------------------------------------" << endl;
   // ------------------------------------------------------------------------ //
   // Begin Transmission of the File!
   // ------------------------------------------------------------------------ //
   while (1) {
      // Check for Duplicates
      if (!duplicate) {
         bytesRead = fread(payload, sizeof(char), PAYLOADSIZE, fs);

         // Check for EOF
         if (bytesRead == 0) 
            break;
         
         //Initialize a New Packet to Send
         char sendPack[PACKETSIZE];
         memset(&sendPack, '\0', sizeof(sendPack));

         //Set Up a New Packet with PACKET STRUCT
         packet pack;
         pack.setSeq(nextSeq);
         pack.setAck(nextAck);
         unsigned char* hold  = createDataPacket(nextSeq, nextAck, clientID, payload, bytesRead);
         memcpy(sendPack, hold, PACKETSIZE);
         
         std::cout << "SEND " << nextSeq << " " << nextAck << " " << clientID << " <CWND> <SS-THRESH> ACK" << std::endl;
         if (sendto(sockfd, sendPack, bytesRead + 12, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
            perror("ERROR: Sendto Failed!");
            return 1;
         }
      }

      //Get Address of Server
      struct sockaddr_in remaddr;
      socklen_t addrlen = sizeof(remaddr);

      //Check for POLLIN (ACKNOWLEDGEMENT FROM SERVER)
      int result = poll(fds, 1, timemax);
      if (result < 0) {
         cerr << "ERROR: Unable to Create Poll" << endl;
         return 1;
      } 
      else if (result > 0) {
         //Check for Any Signs from SERVER
         if (fds[0].revents & POLLIN) {
            //Create a New Packet
            unsigned char recvBuf[PACKETSIZE];

            // Get the Received Bytes
            bytesRecv = recvfrom(sockfd, recvBuf, PACKETSIZE, 0, (struct sockaddr *) &remaddr, &addrlen);

            // Check for Error
            if (bytesRecv < 0) {
                  cerr << "ERROR: recv() failed" << endl;
                  close(sockfd);
                  return 1;
            }

            //Create a New Packet
            packet recvPack(recvBuf, PACKETSIZE);

            cout << "RECV " << recvPack.header.seq << " " << recvPack.header.ack << " " << recvPack.header.connID << " <CWND> <SS-THRESH> ACK" << endl;

            // TODO: Check the ACK Number and Make Sure that it Matches what We Want
            //Check that the Connections are the Same!
            if (recvPack.header.connID == clientID) {
               if (recvPack.header.ack == nextSeq + bytesRead) {
                  // Set Duplicate to False
                  duplicate = false;

                  // Set Next Seq to the Acknowledgement Number of the Server
                  nextSeq = recvPack.header.ack;
                  nextAck = 0;
               } 
               else {
                  cout << "DROP " << recvPack.header.seq << " " << recvPack.header.ack << " " << recvPack.header.connID << " <CWND> <SS-THRESH> ACK" << endl;
                  duplicate = true;
               }
            } 
            else {
               cerr << "ERROR: Received Wrong Connection ID: " << recvPack.header.connID << endl;
               return 1;
            }
         }
      }
      else {
         cerr << "ERROR: ACKNOWLEDGEMENT TIMEOUT" << endl;

         //Resend packet
         fseek(fs, -bytesRead, SEEK_CUR);

         //Reset Duplicate
         duplicate = false;
      }
   }
   // ------------------------------------------------------------------------ //
   // FIN FLAG - DONE SENDING FILE!
   // ------------------------------------------------------------------------ //
   //Create FIN Packet
   cerr << "BEGIN FIN SHUTDOWN--------------------------------------------------" << endl;
   unsigned char sendFin[PACKETSIZE] = {0};
   unsigned char *fin = createFin(nextSeq, clientID);
   memcpy(sendFin, fin, PACKETSIZE);

   while (1) {
      cout << "SEND " << nextSeq << " " << 0 << " " << clientID << " <CWND> <SS-THRESH> FIN" << endl;
      if (sendto(sockfd, sendFin, HEADERSIZE, 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
         cerr << "ERROR: Unable to Send FIN Packet" << endl;
         return 1;
      }

      int result = poll(fds, 1, 10000); //Create a POLL for 10? Seconds
      if (result < 0) {
         cerr << "ERROR: Unable to Create Poll" << endl;
         return 1;
      } 
      else if (result > 0) {
         //Get Address of Server
         struct sockaddr_in remaddr;
         socklen_t addrlen = sizeof(remaddr);
         unsigned char buf[PACKETSIZE];

         //Check for ACK from Server
         bytesRead = recvfrom(sockfd, buf, PACKETSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

         if (bytesRead > 0) {
            packet pack(buf, PACKETSIZE);
            if (pack.getAckFlag()) {
               cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " <CWND> <SS-THRESH> ACK" << endl;

               //Get Address from Server Again
               struct sockaddr_in remaddr;
               socklen_t addrlen = sizeof(remaddr);

               // Start Timer
               clock_t startTime = clock();
               double secondsPassed;
               double secondsToDelay = 2.0;

               bool flag = true;
               while (flag) {
                  // Check that Two seconds Haven't Passed
                  secondsPassed = (clock() - startTime) / CLOCKS_PER_SEC;
                  if (secondsPassed >= secondsToDelay) {
                     cerr << secondsPassed << " Seconds have Passed" << endl;
                     flag = false;
                  }
                  // Create a New Packet
                  unsigned char recvBuf[PACKETSIZE];

                  // Recv from the Server
                  bytesRecv = recvfrom(sockfd, recvBuf, PACKETSIZE, 0, (struct sockaddr *) &remaddr, &addrlen);

                  // Check for No Data
                  if (bytesRecv > 0) {
                     //Check for FIN from Server
                     packet pack(recvBuf, PACKETSIZE);
                     if (pack.getFinFlag()) {
                        cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " <CWND> <SS-THRESH> FIN" << endl;
                        
                        //Respond with ACK
                        unsigned char sendAck[PACKETSIZE] = {0};
                        unsigned char* ack = createAck(nextSeq + 1, pack.header.seq + 1, clientID);
                        memcpy(sendAck,  ack,  PACKETSIZE);

                        //Send the ACK Packet
                        cout << "SEND " << nextSeq + 1 << " " << pack.header.seq + 1 << " " << clientID << " <CWND> <SS-THRESH> ACK" << endl;
                        if (sendto(sockfd, sendAck, HEADERSIZE, 0, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
                           perror("ERROR: sendto Failed");
                           return 0;
                        }
                     }
                  }
               }
               break;
            }
         }
      } 
      else {
         cerr << "ERROR: Waited for More than 10 Seconds" << endl;
      }
   }
   close(sockfd); //Finally Close the Connection
   fclose(fs);
   return 0; //Exit Normally
}