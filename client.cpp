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
uint32_t packetSeq = -1;
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

   // ----------------------------------------------------------------------- //
   // Congestion Control Variables
   // ----------------------------------------------------------------------- //
   int CWND = 512; //Congestion Window Size
	int SSTHRESH = 10000; //Slow Start Threshold
	int current_window = 0; //Current Window Size
	int send_size = PAYLOADSIZE; //How Much We Should Send
   clock_t start_time = (double)clock();

   // ----------------------------------------------------------------------- //
   // THREE WAY HANDSHAKE!!!
   // ----------------------------------------------------------------------- //
   cerr << "START THREE WAY HANDSHAKE-------------------------------------------" << endl;
   while (1) {	
      cout << "SEND " << 12345 << " " << 0 << " " << 0 << " " << CWND << " " << SSTHRESH << " SYN" << endl;
      //Send First HandShake to Server!
      if (sendto(sockfd, sendSyn, HEADERSIZE, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
         perror("ERROR: Sendto Failed!");
         return 0;
      }
      int result = poll(fds, 1, 10000); //Poll 15 Seconds for Response
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
            cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " " << CWND << " " << SSTHRESH << " ACK SYN" << endl;
            start_time = (double)clock();
            // Store Connetion ID into a Global Variable
            clientID = pack.header.connID;
            nextAck = pack.header.seq + 1;
            nextSeq = pack.header.ack;
            packetSeq = nextSeq;

            //Create a ACKNOWLEDGEMENT Packet
            unsigned char sendAck[PACKETSIZE] = {0};
            unsigned char* ack = createAck(pack.header.ack, pack.header.seq + 1, pack.header.connID);
            memcpy(sendAck,  ack,  PACKETSIZE);

            cout << "SEND " << pack.header.ack << " " << pack.header.seq + 1 << " " << pack.header.connID << " " << CWND << " " << SSTHRESH << " ACK" << endl;
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
   bool break_out = false;
   while (1) {
      while (current_window + 512 <= CWND) { //If current window size is filled up, we only wait for ACKS
         send_size = 512;

         // Check for Duplicates
         if (!duplicate) { //TODO: WHY ! DUPLICATE???
            memset(&payload, '\0', sizeof(payload));
            bytesRead = fread(payload, sizeof(char), send_size, fs);
            // Check for EOF
            if (bytesRead == 0) {
               break_out = true;
               break;
            }
            
            //Initialize a New Packet to Send
            char sendPack[PACKETSIZE];
            memset(&sendPack, '\0', sizeof(sendPack));

            //Set Up a New Packet with PACKET STRUCT
            packet pack;
            pack.setSeq(packetSeq % MAXNUM);
            pack.setAck(nextAck % MAXNUM);
            unsigned char* hold  = createDataPacket(packetSeq % MAXNUM, nextAck % MAXNUM, clientID, payload, bytesRead);
            memcpy(sendPack, hold, PACKETSIZE);
            
            std::cout << "SEND " << packetSeq % MAXNUM << " " << 0 << " " << clientID << " " << CWND << " " << SSTHRESH << std::endl;
            if (sendto(sockfd, sendPack, bytesRead + 12, 0, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
               perror("ERROR: Sendto Failed!");
               return 1;
            }
            packetSeq = packetSeq + bytesRead;
            current_window = current_window + send_size; //Update current_window
         }
      }
      if(break_out){
         break;
      }

      double seconds_passed = ((double)(clock() - start_time) / CLOCKS_PER_SEC);
      if ( seconds_passed  > 10.0) {
         cerr << "ERROR: Client did not receive Packet from the Server for more than 10 Seconds" << endl;
         close(sockfd);
         return 3;
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
            while(true) {
               unsigned char recvBuf[PACKETSIZE];

               // Get the Received Bytes
               bytesRecv = recvfrom(sockfd, recvBuf, PACKETSIZE, 0, (struct sockaddr *) &remaddr, &addrlen);

               // Check for Error
               if (bytesRecv <= 0) {
                  break;
               }
               //Create a New Packet
               packet recvPack(recvBuf, PACKETSIZE);
               start_time = (double)clock();
               cout << "HERE3" << endl;

               cout << "RECV " << recvPack.header.seq % MAXNUM << " " << recvPack.header.ack % MAXNUM << " " << recvPack.header.connID << " " << CWND << " " << SSTHRESH << " ACK" << endl ;

               // TODO: Check the ACK Number and Make Sure that it Matches what We Want
               //Check that the Connections are the Same!
               if (recvPack.header.connID == clientID) {
               //cout << "recvPack.header.ack: " << recvPack.header.ack << ": " << nextSeq + bytesRead << "\n";   
            //cout << "free space : " << CWND - current_window;
               if ((recvPack.header.ack % MAXNUM) >= (nextSeq + bytesRead) % MAXNUM) {
                     if(CWND < SSTHRESH){ //Slow Start!
                        CWND = CWND + 512;
                        if(CWND > 51200){
                           CWND = 51200;
                        }
                        //SEND DATA FROM: AFTER THE LAST ACKNOWLEDGED BYTE TO: THE CONGESTION WINDOW SIZE(CWND). Can be split up to multiple packets.
                        current_window = current_window - bytesRead;
                     }
                     else if(CWND >= SSTHRESH){ //Congestion Avoidance!
                        CWND = CWND + (512 * 512) / CWND;
                        if(CWND > 51200){
                           CWND = 51200;
                        }
                        //SEND DATA FROM: AFTER THE LAST ACKNOWLEDGED BYTE TO: THE CONGESTION WINDOW SIZE(CWND). Can be split up to multiple packets.
                        current_window = current_window - bytesRead;
                     }
                     // Set Duplicate to False
                     duplicate = false;

                     // Set Next Seq to the Acknowledgement Number of the Server
                     nextSeq = recvPack.header.ack % MAXNUM;
                     nextAck = 0;
                  } 
                  else {
                     cout << "DROP " << recvPack.header.seq % MAXNUM << " " << recvPack.header.ack % MAXNUM << " " << recvPack.header.connID << " " << CWND << " " << SSTHRESH << " ACK" << endl;
                     duplicate = true;
                  }
               } 
               else {
                  cerr << "ERROR: Received Wrong Connection ID: " << recvPack.header.connID << endl;
                  return 1;
               }
               usleep(3000);
            }
         }
      }
      else {
         cerr << "ERROR: ACKNOWLEDGEMENT TIMEOUT" << endl;

         SSTHRESH = CWND/2;
			CWND = 512;
         //Resend packet
         fseek(fs, -current_window, SEEK_CUR);
         current_window = 0;

         //Reset Duplicate
         duplicate = false;
         double seconds_passed = ((double)(clock() - start_time) / CLOCKS_PER_SEC);
         if ( seconds_passed > 10.0) {
            cerr << "ERROR: Client did not receive Packet from the Server for more than 10 Seconds" << endl;
            close(sockfd);
            return 3;
         }
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
      cout << "SEND " << nextSeq % MAXNUM << " " << 0 << " " << clientID << " " << CWND << " " << SSTHRESH << " FIN" << endl;
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
               cout << "RECV " << pack.header.seq % MAXNUM << " " << pack.header.ack % MAXNUM << " " << pack.header.connID << " " << CWND << " " << SSTHRESH << " ACK" << endl;

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
                        cout << "RECV " << pack.header.seq % MAXNUM << " " << pack.header.ack % MAXNUM << " " << pack.header.connID << " " << CWND << " " << SSTHRESH << " FIN" << endl;
                        
                        //Respond with ACK
                        unsigned char sendAck[PACKETSIZE] = {0};
                        uint32_t server_seq = (nextSeq + 1) % MAXNUM; //(nextSeq + 1 > MAXNUM) ? 0 : nextSeq + 1;
                        uint32_t server_ack = (pack.header.seq + 1) % MAXNUM; //(pack.header.seq + 1 > MAXNUM) ? 0 : pack.header.seq + 1;
                        unsigned char* ack = createAck(server_seq, server_ack, clientID);
                        memcpy(sendAck,  ack,  PACKETSIZE);

                        //Send the ACK Packet
                        cout << "SEND " << server_seq << " " << server_ack << " " << clientID << " " << CWND << " " << SSTHRESH << " ACK" << endl;
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