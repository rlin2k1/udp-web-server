# CS118 Project 2
Group Members and UCLA IDs: Roy Lin(UCLA ID: 704-767-891), Julien Collins(UCLA ID: 804-745-115), Grand Huynh(UCLA ID: 404-814-243)<br>

## Contributions of Each Team Member
Julien: <br>
<br>
Grand: My role was to create the header file and associated helper functions. I wrote the skeleton including command line parsing. I also implemented the three way handshake including retransmitts. Like everyone else, I participated in the debugging sessions that made up a large part of the workload. <br>
<br>
Roy: My role was dealing with the Congestion Avoidance section in the client.cpp file. I wrote the rules on updating the Congestion Window size, and making sure that client sends up to CWND bytes of data. And, after each ACK is received, I either go into Slow Start or Congestion Avoidance Mode. Depending on Timeouts, I update the Slow Start Threshold the the Congestion Window Size. Fixed Bugs in Overflow Issues and SEQ/ACK Numbers differing from what they should be in the Reference Clients and Servers. <br>
<br>
Contributions can be seen in the Git Log. <br>

## High Level Design of Server
The server starts by setting up the initial information associated to the UDP connection that will eventually be established between it and the other clients. Once it has established itself on the chosen port number, as well as set up the receiving sockets by binding the address to the socket, it will enter an infinite while loop that will service the requests. At the top of the loop, it checks for live connections. If the connection is alive, but there has been no response for more than 10 seconds, it will kill the connection and put a single "ERROR" string in the appropriate file. Otherwise, it will continue with the logic. A thing to note is that whenever the server sends a packet to the client, it uses the same remote address since receiving packets from UDP actually fills in the information with the appropriate client information.<br>
After checking for live connections, the first thing it does is receive packets. Once it's received a packet, it will check the packet for 3 things: whether it has a SYN flag, a Fin flag or neither. If it receives a SYN flag, it initiates the three way handshake by creating a SYN ACK package that it sends back to the client, along with information such as its sequence number, its connection ID, its ack number, etc. It also opens the file descriptor for that particular client connection (1.file for the first one, 2.file for the next, etc.). After storing the next expected sequence in an array, it increases the number of connections. <br>
If it receives a FIN flag, it initiates FIN shutdown. First, it closes the file descriptor associated to that client connection. Then it creates a FIN ACK package and sends it to the client.
If it receives a non FIN or non ACK packet, it starts appending the data received to the appropriate file. First, it checks to see if the received sequence number is equal to the expected one (stored server side). If it is, then it proceeds to parse the data from the packet and write it to the file. Once it does that, it sends an acknowledgement to the client and updates the next expected sequence number appropriately. If it's not equal, then it formulates an ACK for the next expected sequence number, and prints that it DROPs that package.<br>
During this process it prints the appropriate error message (such as DROP for incorrect sequence number). If you exit the server through command c, it will close the socket and the server. <br>

## High Level Design of Client

## Problems We Ran Into

## List of Additional Libraries Used
We had quite a bit of included libraries:<br>
#include <sys/types.h><br>
#include <sys/socket.h> //For the Socket()<br>
#include <netinet/in.h><br>
#include <arpa/inet.h><br>
#include <string.h> //String Library<br>
#include <stdio.h> //Standard Input/Ouput Library<br>
#include <errno.h> //Error Handling<br>
#include <unistd.h><br>
#include \<thread\> //For Multi-Threading - Handling Multiple Clients<br>
#include \<iostream\> //Input and Output<br>
#include <signal.h> //For Signal Processing and KILL() Command<br>
#include <fcntl.h> //For Socket Non Blocking<br>
#include \<chrono\> //For Timing<br>
#include <stdlib.h> //For Standard Library<br>
#include <sys/wait.h><br>
#include \<fstream\><br>
#include <sys/select.h> //For the Select()<br>
#include <netdb.h> //For getaddrinfo()
#include <sys/stat.h> //For MakeDir()
#include <sys/time.h>
#include "header.hpp" //Our Own Library for Packet Creation
#include <poll.h> //For Polling Use
#include <ctime> //timing
#include <cstdlib>
    
## Online Tutorials or Code Examples
Code Examples:
- https://www.geeksforgeeks.org/udp-server-client-implementation-c/
- https://www.cs.rutgers.edu/~pxk/417/notes/sockets/udp.html
- https://linux.m2osw.com/c-implementation-udp-clientserver

Online Tutorials:
- https://en.wikipedia.org/wiki/Berkeley_sockets
- http://beej.us/guide/bgnet/
- https://www.cs.dartmouth.edu/~campbell/cs60/socketprogramming.html
- https://dzone.com/articles/parallel-tcpip-socket-server-with-multi-threading
- http://man7.org/linux/man-pages/man2/select.2.html
- https://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/
- https://stackoverflow.com/questions/1352749/multiple-arguments-to-function-called-by-pthread-create
- http://www.cplusplus.com/reference/cstdio/freopen/
- https://stackoverflow.com/questions/5269683/rewindfile-pointer-and-then-fwrite-wont-fwrite-overwrite-the-file-contents
- http://www.cplusplus.com/reference/cstdio/fopen/
- http://developerweb.net/viewtopic.php?id=3196
- https://en.wikibooks.org/wiki/C_Programming/POSIX_Reference/netdb.h/getaddrinfo#Example
- https://stackoverflow.com/questions/16372700/how-to-use-getaddrinfo-to-connect-to-a-server-using-the-external-ip
- https://docs.microsoft.com/en-us/windows/desktop/api/ws2tcpip/nf-ws2tcpip-getaddrinfo
- https://www.dreamincode.net/forums/topic/109330-how-to-use-getaddrinfo/

## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` and `client` executables.

It provides a `clean` target, and `tarball` target to create the submission file as well.

You will need to modify the `Makefile` to add your userid for the `.tar.gz` turn-in at the top of the file.

## Provided Files

`server.cpp` and `client.cpp` are the entry points for the server and client part of the project.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.

## Wireshark dissector

For debugging purposes, you can use the wireshark dissector from `tcp.lua`. The dissector requires
at least version 1.12.6 of Wireshark with LUA support enabled.

To enable the dissector for Wireshark session, use `-X` command line option, specifying the full
path to the `tcp.lua` script:

    wireshark -X lua_script:./confundo.lua

To dissect tcpdump-recorded file, you can use `-r <pcapfile>` option. For example:

    wireshark -X lua_script:./confundo.lua -r confundo.pcap
