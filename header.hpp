/* header.hpp
Header File for PACKET STRUCT

Author(s):
  Grand Huynh, Julien Collins, Roy Lin

Date Created:
  March 1st, 2019
*/
#include <bitset>
#include <netinet/in.h>

#define PACKETSIZE 524
#define PAYLOADSIZE 512
#define HEADERSIZE 12
#define MAXNUM 102401

struct packet_header {
	uint32_t  seq;
	uint32_t ack;
	uint16_t connID;
	uint16_t flags;
};

struct packet {
	packet_header header;
	unsigned char buf[524];
	
	//Helper Functions
	packet();
	packet(unsigned char* packet, int packetSize);
	void setSeq(uint32_t seqnum);
	void setAck(uint32_t acknum);
	void setConnID(uint16_t connID);
	void setAckFlag();
	void setSynFlag();
	void setFinFlag();
	unsigned char* createPacket(unsigned char * payload, int payloadSize);
	void readPacket();
	bool getAckFlag();
	bool getSynFlag();
	bool getFinFlag();
};

packet::packet(){
	header.seq = 0;
	header.ack = 0;
	header.connID =0;
	header.flags = 0;
   memset(&buf, '\0', sizeof(buf));
}

packet::packet(unsigned char* pack, int packetSize){
	//TODO: Fix 
	//Must Be Creating a Full Packet for Now
	if (packetSize != PACKETSIZE) {
		return;
	}

	//Set header struct based on bits sent in beginning of packet
	memcpy(&header.seq , (uint32_t*) pack, 4);
	header.seq = ntohl(header.seq);
	
	memcpy(&header.ack , (uint32_t*) (pack + 4) , 4);
	header.ack = ntohl(header.ack);
	
	memcpy(&header.connID, (uint32_t*) (pack + 8), 2);
	header.connID= ntohs(header.connID);
	
	memcpy(&header.flags , (uint32_t*) (pack + 10),  2);
	header.flags = ntohs(header.flags);
	
	//store copy of this buf inside
	memcpy((unsigned char*) buf  , (unsigned char*) pack , packetSize);
}

void packet::setSeq(uint32_t seqnum){
	header.seq = seqnum;
}

void packet::setAck(uint32_t acknum){
	header.ack = acknum;
}

void packet::setConnID(uint16_t connID){
	header.connID = connID;
}

void packet::setFinFlag(){
	header.flags |= 0x0001;
}

void packet::setSynFlag(){
	header.flags |= 0x0002;
}

void packet::setAckFlag(){
	header.flags |= 0x0004;
}

bool packet::getFinFlag(){
	if(header.flags & 0x0001){
		return true;
	}
	return false;
}

bool packet::getSynFlag(){
	if(header.flags & 0x0002){
		return true;
	}
	return false;
}

bool packet::getAckFlag(){
	if(header.flags & 0x0004){
		return true;
	}
	return false;
}

unsigned char*  packet::createPacket(unsigned char* payload, int payloadSize){
	
	//std::cout << "HERE IS PAYLOAD IN HEADER : " << payload;
	//Zero out current buffer
	memset(buf, '\0', PACKETSIZE);
	
	//Payload cannot be too big.
	if (payloadSize > 512) {	
		return NULL;
	}

	unsigned char head[12];
	//Place header in the beginning of the packet
	head[0] = (header.seq >> 24) & 0xFF;
	head[1] = (header.seq >> 16) & 0xFF;
	head[2] = (header.seq >> 8) & 0xFF;
	head[3]  = header.seq & 0xFF;

	head[4] = (header.ack >> 24) & 0xFF;
	head[5] = (header.ack >> 16) & 0xFF;
	head[6] = (header.ack >> 8) & 0xFF;
	head[7]  = header.ack & 0xFF;
	
	head[8] = (header.connID>> 8) & 0xFF;
	head[9] = (header.connID >> 0) & 0xFF;
	
	head[10] = (header.flags >> 8) & 0xFF;
	head[11]  = header.flags & 0xFF;
	memcpy(buf  , (unsigned char*) head , 12);

	//Fill remaining bytes with data
	memcpy((unsigned char*) buf  + 12, (unsigned char*) payload , payloadSize);
	
	//std::cout << "HERE IS BUF: " << buf << "\n";
	return buf;
}

unsigned char* createSynAck(uint16_t connid){
	packet pack;
	pack.setSeq(4321);
	pack.setAck(12346);
	pack.setConnID(connid);
	//printf("CONNID : %u", connid);
	pack.setSynFlag();
	pack.setAckFlag();
	
	unsigned char payload[PAYLOADSIZE]; 
	memset(&payload, '\0', sizeof(payload));
	return pack.createPacket(payload, 0);
}

unsigned char* createSyn(){
	packet pack;
	pack.setSeq(12345);
	pack.setAck(0);
	pack.setConnID(0);
	pack.setSynFlag();
	
	unsigned char payload[PAYLOADSIZE]; 
	memset(&payload, '\0', sizeof(payload));
	return pack.createPacket(payload, 0);
}

unsigned char* createDataPacket(uint32_t seq, uint32_t ack, uint16_t connID, unsigned char *payload, int payloadSize) {
   // Create data packet
   packet pack;
   pack.setSeq(seq);
   pack.setAck(ack);
   pack.setConnID(connID);
   
   // Create payload and return entire packet
   return pack.createPacket(payload, payloadSize);
}

unsigned char* createAck(uint32_t seq, uint32_t ack, uint16_t connID ){
	packet pack;
	pack.setSeq(seq);
	pack.setAck(ack);
	pack.setConnID(connID);
	pack.setAckFlag();
	
	unsigned char payload[PAYLOADSIZE];
	memset(&payload, '\0', sizeof(payload));
	return pack.createPacket(payload, 0);
}

unsigned char* createFin(uint32_t seq, uint16_t connID){
	packet pack;
	pack.setSeq(seq);
	pack.setAck(0);
	pack.setConnID(connID);
	pack.setFinFlag();
	
	unsigned char payload[PAYLOADSIZE]; 
	memset(&payload, '\0', sizeof(payload));
	return pack.createPacket(payload, 0);
}
