/* server.cpp
   The server listens for TCP connections and saves all the received data from
   the client in a file.

Notes: Mainly use select() for 15 Second Timer!

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
#include <sstream> //String Stream

#include <signal.h> //For Signal Processing and KILL() Command
#include <fcntl.h> //For Socket Non Blocking
#include <chrono> //For Timing

#include <stdlib.h> //For Standard Library
#include <sys/wait.h>

#include <fstream>
#include <sys/select.h> //For the Select()

#include <sys/stat.h> //For MakeDir()
#include <netdb.h> //For GetHostByName()
#include <sys/time.h>

#include "header.hpp" 

using namespace std; //Using the Standard Namespace

#define MAXCLIENTS 30
int num_conn = 1;
uint32_t conn_state[MAXCLIENTS];
FILE *files[MAXCLIENTS];
clock_t times[MAXCLIENTS];
bool is_valid[MAXCLIENTS] = {false};
bool shut_down[MAXCLIENTS] = {false};

string file_directory = "";
char error[6] = "ERROR";

void sigquit_handler(int signum) {
   cerr << "ERROR: Received SIGQUIT Signal" << endl;
   exit(0);
}

void sigterm_handler(int signum) {
   cerr << "ERROR: Received SIGTERM Signal" << endl;
   exit(0);
}

int main(int argc, char *argv[]) //Main Function w/ Arguments from Command Line
{
   // ------------------------------------------------------------------------ //
   // Signal Handling
   // ------------------------------------------------------------------------ //
   signal(SIGQUIT, sigquit_handler);
   signal(SIGTERM, sigterm_handler);
   int port_number = -1; //Sentinel
   // ------------------------------------------------------------------------ //
   // Error Handling from Arguments
   // ------------------------------------------------------------------------ //
   if(argc != 3) {
      cerr << "ERROR: Need 2 Arguments: Port Number and File Directory" << endl;
      exit(3);
   }
   try{
      port_number = stoi(argv[1]); //The Port Number is the First Argument
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
   file_directory = argv[2]; //Assume Directory is Always Correct-2nd Arg
   mkdir(file_directory.c_str(), 0777); //Will Always Have Permissions

   //cerr << "Port Number to Be Connected to: " << port_number << endl;
   //cerr << "Directory to Save Transferred File in: " << file_directory << endl;

   // ------------------------------------------------------------------------ //
   // Create a Socket using TCP IP
   // ------------------------------------------------------------------------ //
   int sockfd; //Socket File Descriptor
   struct addrinfo hints, *servinfo, *p; //For getaddrinfo()
   int rv; //Check Return Value of getaddrinfo()

   memset(&hints, 0, sizeof hints); //Initialize Hints
   hints.ai_family = AF_INET; // AF_INET for IPv4
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags = AI_PASSIVE; // Use My IP address

   if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) { //GetAddrInfo
      cerr << "ERROR: Get Address Info Failed" << endl;
      exit(3);
   }

   // Loop through all results and try to bind
   for(p = servinfo; p != NULL; p = p->ai_next) {
      //sockfd contains the file descriptor to access the Socket
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
      { //Create Socket for the Result
         cerr << "ERROR: Failed to Create Socket" << endl;
         continue; //On to the Next Result
      }
      // -------------------------------------------------------------------- //
      // Allow Others to Reuse the Address - Error Handling
      // -------------------------------------------------------------------- //
      int yes = 1;
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
      {
         cerr << "ERROR: Set Socket Options Failed" << endl;
      }
      fcntl(sockfd, F_SETFL, O_NONBLOCK);
      // -------------------------------------------------------------------- //
      // Bind Address to Socket
      // -------------------------------------------------------------------- //
      if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
         cerr << "ERROR: Error Binding the Port and the HostName/IP Address" <<
            "Together" << endl;
         close(sockfd); //Finally Close the Connection
         continue;
      }

      break; // Must have connected successfully
   }

   if (p == NULL) {
      // End of List with No Successful Bind
      cerr << "ERROR: Failed to Bind Socket" << endl;
      exit(3);
   }

   freeaddrinfo(servinfo); // Deallocate Structure

   //Address of Client Connection
   struct sockaddr_in remaddr;
   socklen_t addrlen = sizeof(remaddr);

   unsigned char buf[PACKETSIZE] = {0};
   int bytesRead = -1;
   // ------------------------------------------------------------------------ //
   // Begin Getting Data from Clients
   // ------------------------------------------------------------------------ //
   while (1) {
      for(int i = 1; i <= num_conn - 1; i++){
         if(is_valid[i] == false)
            continue;
         if( ((clock() - times[i]) / CLOCKS_PER_SEC) > 10.0) {
            string file_path = file_directory + "/" + to_string(i) + ".file"; //Filename
            FILE *fd = freopen(file_path.c_str(), "w", files[i]); //Rewrite the File!
            fwrite(error, 1, 6, fd); //Write the Error Message into the File
            fflush(fd); //Make Sure Everything is Written to File!
            is_valid[i] = false;
         }
      }
      memset(buf, '\0', sizeof(buf)); //Clear the Receiving Buffer
      bytesRead = recvfrom(sockfd, buf, PACKETSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);

      if (bytesRead > 0) {
         packet pack(buf, PACKETSIZE); //Create a New Packet that Just Arrived
         if (pack.getSynFlag()){ //Create New Connection if Never Seen Before
            cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " SYN" << endl;
            conn_state[num_conn] = pack.header.seq + 1; //Save Expected Sequence Number

            //Respond to Create an SYN + ACKNOWLEDGEMENT PACKET
            unsigned char sendSynAck[PACKETSIZE] = {0};
            unsigned char* synAck = createSynAck((uint16_t)num_conn);
            memcpy(sendSynAck, synAck, PACKETSIZE);

            //Send out Syn + Ack PACKET
            cout << "SEND " << 4321 << " " << 12346 << " " << num_conn << " ACK SYN" << endl;
            if (sendto(sockfd, sendSynAck, HEADERSIZE, 0, (struct sockaddr *)&remaddr, addrlen) < 0) {
               perror("ERROR: sendto() Failed");
               return 0;
            }

            //Create File Handler
            string file_path = file_directory + "/" + to_string(num_conn) + ".file"; //FileName
            times[num_conn] = clock();
            FILE *fs = fopen(file_path.c_str(), "wb"); //Open the File for Modification
            files[num_conn] = fs;
            is_valid[num_conn] = true;
            if (!files[num_conn]) {
               cerr << "ERROR: Could Not Open File" << endl;
               return 1;
            }

            //Increase Number of Connections
            num_conn++;
         } else if (pack.getFinFlag()) { //Start Shutdown Sequence
            if(is_valid[pack.header.connID] == false)
               cerr << "ERROR: PACKET CONNECTION ID IS NOT VALID" << endl;
            cout << "RECV " << pack.header.seq << " " << pack.header.ack << " " << pack.header.connID << " FIN" << endl;

            // Create ACKNOWLEDGEMENT Packet to Send Back
            unsigned char sendAck[PACKETSIZE] = {0};
            uint32_t client_ack = (pack.header.seq + 1) % MAXNUM; //(pack.header.seq + 1 > MAXNUM) ? 0 : pack.header.seq + 1;
            unsigned char *ack = createAck(4322, client_ack, pack.header.connID);
            memcpy(sendAck, ack, PACKETSIZE);

            // Send the ACKNOWLEDGEMENT Packet
            cout << "SEND " << 4322 << " " << client_ack << " " << pack.header.connID << " ACK" << endl;
            if (sendto(sockfd, sendAck, HEADERSIZE, 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
               perror("ERROR: FIN ACK FAILED!");
               return 1;
            }

            // Close File Descriptor
            int conn = (int)pack.header.connID;
            fclose(files[conn]);

            // ALso Create FIN?
            unsigned char sendFin[PACKETSIZE] = {0};
            unsigned char *fin = createFin(4322, pack.header.connID);
            memcpy(sendFin, fin, PACKETSIZE);

            // Send the FIN Packet
            cout << "SEND " << 4322 << " " << 0 << " " << pack.header.connID << " FIN" << endl;
            if (sendto(sockfd, sendFin, HEADERSIZE, 0, (struct sockaddr *) &remaddr, sizeof(remaddr)) < 0) {
               cerr << "ERROR: Unable to Send FIN Packet" << endl;
               return 1;
            }
            cerr << "FILE DONE TRANSMITTING---------------------------------------" << endl;
				shut_down[pack.header.connID] = true;
         } else { //There is More Data: Save into File!
            if(is_valid[pack.header.connID] == false)
               cerr << "ERROR: PACKET CONNECTION ID IS NOT VALID" << endl;
            if (!pack.getAckFlag()) {
               // Packet variables
               int conn = (int) pack.header.connID;

               // Check for correct nums
               if (conn_state[conn] % MAXNUM == pack.header.seq % MAXNUM) {
                  char test[PAYLOADSIZE];
                  memset(&test, '\0', sizeof(test));
                  memcpy(test, buf + 12, PAYLOADSIZE);

                  int fileBytesWritten = fwrite(test, sizeof(char), bytesRead - 12, files[conn]); //Write File
                  fflush(files[conn]); //Make Sure Everything is Written to File!

                  //Send an ACK
                  unsigned char sendAck[PACKETSIZE] = {};
                  uint32_t client_ack = (pack.header.seq + fileBytesWritten) % MAXNUM; //(pack.header.seq + fileBytesWritten > MAXNUM) ? 0 : pack.header.seq + fileBytesWritten;
                  unsigned char *ack = createAck(4322, client_ack, pack.header.connID);
                  memcpy(sendAck, ack, PACKETSIZE);

                  //Send the ACKNOWLEDGEMENT PACKET TO CLIENT
                  cout << "SEND " << 4322 << " " << client_ack << " " << pack.header.connID << " ACK" << endl;
                  if (sendto(sockfd, sendAck, HEADERSIZE, 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
                     perror("ERROR: sendto() Failed");
                     return 1;
                  }
                  //Update the Expected Sequence Number
                  conn_state[conn] = (pack.header.seq + fileBytesWritten) % MAXNUM;
                  times[conn] = clock();
               } else if (conn_state[conn] % MAXNUM > pack.header.seq % MAXNUM){
                  //Send an ACK
                  unsigned char sendAck[PACKETSIZE] = {};
                  unsigned char *ack = createAck(4322, conn_state[conn] % MAXNUM, pack.header.connID);
                  memcpy(sendAck, ack, PACKETSIZE);

                  //Send the ACKNOWLEDGEMENT PACKET TO CLIENT
                  cout << "SEND " << 4322 << " " << conn_state[conn] % MAXNUM << " " << pack.header.connID << " ACK" << endl;
                  if (sendto(sockfd, sendAck, HEADERSIZE, 0, (struct sockaddr *) &remaddr, addrlen) < 0) {
                     perror("ERROR: sendto() Failed");
                     return 1;
                  }

               } else {
                  cerr << (int) pack.header.seq << ":" << conn_state[conn] << "\n";
               }
            } else {
               cout << "RECV " << pack.header.seq % MAXNUM << " " << pack.header.ack % MAXNUM << " " << pack.header.connID << " ACK" << endl;
					if(shut_down[pack.header.connID])
						is_valid[pack.header.connID] = false;
				}
         }
      }
   }

   // Close socket
   close(sockfd);
   return 0;
}