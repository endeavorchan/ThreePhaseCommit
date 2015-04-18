#include "msg.h"
	
MSG::MSG(char *msgport, char *hostfile){
	//cout << "initial MSG " << endl;

	struct sockaddr_in saddr;

  	setmyIp();
  	setmyPort(msgport);
  	//cout <<"c " << inet_ntoa(*(struct in_addr*)&my_ip) << endl;
  	this->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  	memset(&saddr, 0, sizeof(saddr));
  	saddr.sin_family = AF_INET;
  	saddr.sin_addr.s_addr = ip;
  	//cout << "my port is " << myport << endl;
  	saddr.sin_port = this->port;
  	bind(sockfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr));

  	string file_name = hostfile;
	ifstream infile(file_name.c_str(),ios::in);
	string textline;
	int index = 0;
	while(getline(infile,textline,'\n')){    
		this->iplist[index++] = textline;
	}  
	this->size = index; 
	//cout << "there are " << index << " nodes" << endl;
	infile.close();
	//IPList::iterator pos;
	/*
	for(pos = iplist.begin(); pos != iplist.end(); ++pos){
		cout << pos->first << " " << pos->second << endl;
	}
	*/
	myid = getidbyIp(myipstr);
}

void MSG::sendAllMsg(int type, char *p) {
	for (int i = 0; i < size; ++i) {
		sendMessage(type, p, i);
	}
}

void MSG::sendMessage(int type, char *p, int dest_id){

	uint32_t dest_ip;
	struct sockaddr_in saddr;
	IPList::iterator pos = iplist.find(dest_id);
	struct hostent *host = gethostbyname(pos->second.c_str());
	memcpy(&dest_ip, host->h_addr_list[0], host->h_length);
	//cout << "the ip of the dest is "<< pos->second << endl;
	char buf[MAXBUFLEN];
	memset(buf, 0, MAXBUFLEN);
	memset(&saddr, 0, sizeof(saddr));
  	saddr.sin_family = AF_INET;
  	saddr.sin_addr.s_addr = dest_ip;
  	saddr.sin_port = port;
  	//cout << "sending port is " << myport << endl;
  	int datasize = 0;
  	if (type == CANCMT) {
  		datasize = sizeof(Cancmt);
  	//	Cancmt *msg = (Cancmt *)p;
  	//	delete msg;

  	} else if (type == REPLY) {
  		datasize = sizeof(Reply);
  	//	Reply *msg = (Reply *)p;
  	//	delete msg;

  	} else if (type == PRECMT) {
  		datasize = sizeof(PreCommit);
  	//	PreCommit *msg = (PreCommit *)p;
  	//	delete msg;

  	} else if (type == ACK) {
  		datasize = sizeof(Ack);
  	//	Ack *msg = (Ack *)p;
  	//	delete msg;

  	} else if (type == DOCMT) {
  		datasize = sizeof(Docommit);
  	//	Docommit *msg = (Docommit *)p;
  	//	delete msg;

  	} else if (type == HAVECMTED) {
  		datasize = sizeof(HaveCommitted);
  	//	HaveCommitted *msg = (HaveCommitted *)p;
  	//	delete msg;

  	} else if (type == ABORT) {
  		datasize = sizeof(Abort);

  	} else if (type == ABORTACK) {
  		datasize = sizeof(AbortAck);
  	} else {
		cout << "message content error\n";
		fflush(stdout);
		exit(1);
	}

  	memcpy(buf, p, datasize);
	if ((sendto(sockfd, buf, datasize, 0, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in))) == -1) {
  		perror("talker: sendto");
		exit(1);
  	}
}


int  MSG::recvMessage(char *&pmsg){
	//struct sockaddr_storage their_addr;
	static char buf[MAXBUFLEN];
	//socklen_t addr_len;
	//int numbytes;
	memset(buf, 0, MAXBUFLEN);
	//printf("listener: waiting to recvfrom...\n");
	struct sockaddr_in addr;
  	socklen_t addrlen = sizeof(struct sockaddr_in);
  	int bytes = recvfrom(sockfd, buf, MAXBUFLEN, 0, (struct sockaddr*)&addr, &addrlen);
  	if(bytes == -1){
  		perror("recvfrom error\n");
		return -1;
  	}

	strncpy(sderipstr, inet_ntoa(addr.sin_addr), INET_ADDRSTRLEN);   // get the ip addr of the process who sent the msg not needed

	uint32_t *ptype = (uint32_t *)buf;
	pmsg = (char *)buf;
	printMsg(R, pmsg);
	if ((*ptype) == CANCMT) {
		return CANCMT;
		
	} else if ((*ptype) == REPLY) {
		return REPLY;

	} else if ((*ptype) == PRECMT) {
		return PRECMT;

	} else if ((*ptype) == ACK) {
		return ACK;
	
	} else if ((*ptype) == DOCMT) {
		return DOCMT;

	} else if ((*ptype) == HAVECMTED) {
		return HAVECMTED;

	} else if ((*ptype) == ABORT) {
		return ABORT;
	} else if ((*ptype) == ABORTACK) {
		return ABORTACK;
	} else {
		cout << "received message error" << endl;
		return -1;
	}
	return -1;
}
