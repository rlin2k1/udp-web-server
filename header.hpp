#include <bitset>
#include <netinet/in.h>

#define PACKETSIZE 524

struct packet_header {
	uint32_t  seq;
	uint32_t ack;
	uint16_t connID;
	uint16_t flags;
};

struct packet {
	packet_header header;
	unsigned char buf[524];
	
	//helper func
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
};

packet::packet(){
	header.seq = 4321;
	header.ack = 4322;
	header.connID =2;
	header.flags = 3;
}

packet::packet(unsigned char* pack, int packetSize){
	//TODO: Fix 
	//Must be creating a full packet for now
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
	
	std::cout << header.seq << ", " <<  header.ack << ", " <<  header.connID << ", " <<  header.flags << "\n";
	std::cout << pack + 12 ;
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



unsigned char*  packet::createPacket(unsigned char* payload, int payloadSize){
	
	std::cout << "HERE IS PAYLOAD IN HEADER : " << payload;
	//Zero out current buffer
	memset(buf, '\0', PACKETSIZE);
	
	if (payloadSize > 512 || payload == NULL) {	
		return NULL;
	}

	unsigned char head[12];
	//Place header in the beginning of the packet
	head[0] = (header.seq >> 24) & 0xFF;
	head[1] = (header.seq >> 16) & 0xFF;
	head[2] = (header.seq >> 8) & 0xFF;
	head[3]  = header.seq & 0xFF;
	//std::cout << "HERE IS BUF: " << head[3] << "\n";

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
	
	std::cout << "HERE IS BUF: " << buf << "\n";
	return buf;
}


