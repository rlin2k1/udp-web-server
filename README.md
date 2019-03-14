# CS118 Project 2
Group Members and UCLA IDs: Roy Lin(UCLA ID: 704-767-891), Julien Collins(UCLA ID: 804-745-115), Grand Huynh(UCLA ID: 404-814-243)<br>

## Contributions of Each Team Member

## High Level Design of Server

The server starts by setting up the initial information associated to the UDP connection that will eventually be established between it and the other clients. Once it has established itself on the chosen port number, as well as set up the receiving sockets by binding the address to the socket, it will enter an infinite while loop that will service the requests. At the top of the loop, it checks for live connections. If the connection is alive, but there has been no response for more than 10 seconds, it will kill the connection and put a single "ERROR" string in the appropriate file. Otherwise, it will continue with the logic. A thing to note is that whenever the server sends a packet to the client, it uses the same remote address since receiving packets from UDP actually fills in the information with the appropriate client information.
After checking for live connections, the first thing it does is receive packets. Once it's received a packet, it will check the packet for 3 things: whether it has a SYN flag, a Fin flag or neither. If it receives a SYN flag, it initiates the three way handshake by creating a SYN ACK package that it sends back to the client, along with information such as its sequence number, its connection ID, its ack number, etc. It also opens the file descriptor for that particular client connection (1.file for the first one, 2.file for the next, etc.). After storing the next expected sequence in an array, it increases the number of connections. 
If it receives a FIN flag, it initiates FIN shutdown. First, it closes the file descriptor associated to that client connection. Then it creates a FIN ACK package and sends it to the client.
If it receives a non FIN or non ACK packet, it starts appending the data received to the appropriate file. First, it checks to see if the received sequence number is equal to the expected one (stored server side). If it is, then it proceeds to parse the data from the packet and write it to the file. Once it does that, it sends an acknowledgement to the client and updates the next expected sequence number appropriately. If it's not equal, then it formulates an ACK for the next expected sequence number, and prints that it DROPs that package.
During this process it prints the appropriate error message (such as DROP for incorrect sequence number). If you exit the server through command c, it will close the socket and the server. 

## High Level Design of Client

## Problems We Ran Into

## List of Additional Libraries Used

## Online Tutorials or Code Examples
Code Examples:
- 

Online Tutorials:
-

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
