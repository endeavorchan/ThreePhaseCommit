#ifndef MSG_H
#define MSG_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <map>
#include <vector>
#define MAXBUFLEN 1024
using namespace std;
typedef map<int, string> IPList;
enum MsgType {
	CANCMT = 1,
	REPLY,
	PRECMT,
	ACK,
	DOCMT,
	HAVECMTED,
	ABORT,
	ABORTACK,
	ALIVE,
	VICTORY,
	INQUIRY,
	COMMITTABLE,
	NONCOMMITTABLE,
	TERMIABORT,
	TRANSIT
};
enum {
	A, C
};
enum {
	S, R
};

typedef struct {
	MsgType type;
	uint32_t val;
	uint32_t senderid;

} Cancmt;

typedef struct {
	MsgType type;
	uint32_t val;
	uint32_t senderid;
	uint32_t isyes; // 1 for yes, 0 for no

} Reply;

typedef struct {
	MsgType type;
	uint32_t val;
	uint32_t senderid;

} PreCommit;

typedef struct {
	MsgType type;
	uint32_t val;
	uint32_t senderid;

} Ack;

typedef struct {
	MsgType type;
	uint32_t val;
	uint32_t senderid;

} Docommit;

typedef struct {
	MsgType type;
	uint32_t val;
	uint32_t senderid;
} HaveCommitted;

typedef struct {
	MsgType type;
	uint32_t val;
	uint32_t senderid;
} Abort;

typedef struct {
	MsgType type;
	uint32_t val;
	uint32_t senderid;
} AbortAck;

typedef struct {
	MsgType type;
	uint32_t val;
	uint32_t senderid;
} Election;

typedef struct {
	MsgType type;
	uint32_t val;
	uint32_t senderid;
} Termination;

class Tag {
	int size;
	bool *tag;
	int *ids;
	int myid;
	int *sessionv;
public:
	Tag() {
	}
	Tag(int size, int myid) {
		this->size = size;
		tag = new bool[size];
		ids = new int[size];
		for (int i = 0; i < size; ++i) {
			tag[i] = false;
			ids[i] = -1;
		}
		this->myid = myid;
		sessionv = new int[size];
		for (int i = 0; i < size; ++i) {
			sessionv[i] = 1;  // 1 indicate the corresponding site is on
		}
	}
	void printIds(){
		cout<< "ids:" <<endl;
		for (int i=0; i<size; i++){
			cout<< ids[i];
		}
		cout<< endl;
	}
	void setsitedown() {
		for (int i = 0; i < size; ++i) {
			if (ids[i] != -1) {
				siteDown(i);
			}
		}
	}
	void siteDown(int id) {
		sessionv[id] = 0;
		setTrue(id);
	}
	bool isDown(int id) {
		if (sessionv[id] == 0) {
			return true;
		} else {
			return false;
		}
	}
	/* if need to set/reset tag,  the tag of the crashed site is always true */
	void filpAll() {
		for (int i = 0; i < size; ++i) {
			tag[i] = !tag[i];
			if (sessionv[i] == 0) {
				setTrue(i);
			}
		}

	}
	void setAlltoFalse() {
		for (int i = 0; i < size; ++i) {
			tag[i] = false;
			if (sessionv[i] == 0) {
				setTrue(i);
			}
		}
	}
	int *getUnsetId() { // get the id of the site which hasn't replied yet (re transmit)
		for (int i = 0; i < size; ++i) {
			ids[i] = -1;
			if (i != myid && tag[i] == false) {
				ids[i] = i;
			}
		}
		return ids;
	}
	void setTrue(int pos) {
		if (pos >= size) {
			cout << "set bit out of bound exit" << endl;
			exit(1);
		} else {
			tag[pos] = true;
		}
	}

	bool checkAllTrue() {
		for (int i = 0; i < size; ++i) {
			if (i != myid && tag[i] == false)
			return false;
		}
		return true;
	}
};

class MSG {
public:
	Tag *tag;  // used to record reply status
	int sockfd;// socked number
	uint16_t port;// port number
	uint32_t ip;// ip number
	char myipstr[INET_ADDRSTRLEN];// my ip in string format
	IPList iplist;// all the ip stored in this map
	int size;// number of hosts in the system
	int myid;// my id
	char sderipstr[INET_ADDRSTRLEN];// sender's ip in string format
	MSG() {
	}
	MSG(char *, char *);
	int getidbyIp(char *s) {
		for (auto pos = iplist.begin(); pos != iplist.end(); ++pos) {
			char temp[INET_ADDRSTRLEN];
			uint32_t my_ip;
			struct hostent *host = gethostbyname(pos->second.c_str()); // convert string to const char *
			memcpy(&my_ip, host->h_addr_list[0], host->h_length);
			strncpy(temp, inet_ntoa(*(struct in_addr*)&my_ip), INET_ADDRSTRLEN);

			if (!strncmp(temp, s, INET_ADDRSTRLEN))
			return pos->first;
		}
		return -1;
	}

	void sendMessage(int type, char *p, int dest_id);
	void sendAllMsg(int type, char *p);
	int recvMessage(char *&pmsg);
	void setmyPort(char *portstr) {
		uint16_t portnum = atoi(portstr);
		this->port = htons(portnum);
	}
	void setmyIp() {
		char name[256];
		gethostname(name, 255);
		struct hostent *host = gethostbyname(name);
		//cout << "my host name is "<< name << endl;
		memcpy(&ip, host->h_addr_list[0], host->h_length);
		strncpy(myipstr, inet_ntoa(*(struct in_addr*)&ip), INET_ADDRSTRLEN);
		//cout << "my ip is " << ipstr <<endl;

	}

	char * mkMsg(int type, uint32_t val, uint32_t senderid, uint32_t isyes = 1) {
		char *ret = NULL;
		switch (type) {
			case CANCMT: {
				Cancmt *msg = new Cancmt;
				msg->type = CANCMT;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case REPLY: {
				Reply *msg = new Reply;
				msg->type = REPLY;
				msg->val = val;
				msg->senderid = senderid;
				msg->isyes = isyes;
				ret = (char *)msg;
				break;
			}
			case PRECMT: {
				PreCommit *msg = new PreCommit;
				msg->type = PRECMT;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case ACK: {
				Ack *msg = new Ack;
				msg->type = ACK;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case DOCMT: {
				Docommit *msg = new Docommit;
				msg->type = DOCMT;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case HAVECMTED: {
				HaveCommitted *msg = new HaveCommitted;
				msg->type = HAVECMTED;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case ABORT: {
				Abort *msg = new Abort;
				msg->type = ABORT;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case ABORTACK: {
				AbortAck *msg = new AbortAck;
				msg->type = ABORTACK;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case ALIVE: {
				Election *msg = new Election;
				msg->type = ALIVE;
				msg->val = 9999;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case VICTORY: {
				Election *msg = new Election;
				msg->type = VICTORY;
				msg->val = 9999;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case INQUIRY: {
				Election *msg = new Election;
				msg->type = INQUIRY;
				msg->val = 9999;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case COMMITTABLE: {
				Termination *msg = new Termination;
				msg->type = COMMITTABLE;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case NONCOMMITTABLE: {
				Termination *msg = new Termination;
				msg->type = NONCOMMITTABLE;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case TERMIABORT: {
				Termination *msg = new Termination;
				msg->type = TERMIABORT;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			case TRANSIT: {
				Termination *msg = new Termination;
				msg->type = TRANSIT;
				msg->val = val;
				msg->senderid = senderid;
				ret = (char *)msg;
				break;
			}
			default: {
				cout << "wrong type" << endl;
				exit(1);
			}

		}
		return ret;
	}

	/* for test */
	void printMsg(int sr, char *msg, int dest = -1) {
		uint32_t *ptype = (uint32_t *)msg;
		if (sr == R) {
			cout << "RRRRR  ";
		} else if (sr == S) {
			cout << "SSSSS  dest:" << dest << " ";
		}
		switch (*ptype) {
			case CANCMT: {
				Cancmt *p = (Cancmt *)msg;
				cout << "CANCMT  val: " << p->val << " senderid: " << p->senderid << endl;
				break;
			}
			case REPLY: {
				Reply *p = (Reply *)msg;
				cout << "REPLY  val: " << p->val << " senderid: " << p->senderid << "  yes/no: " << p->isyes << endl;
				break;
			}
			case PRECMT: {
				PreCommit *p = (PreCommit *)msg;
				cout << "PRECMT val: " << p->val << " senderid: " << p->senderid << endl;
				break;
			}
			case ACK: {
				Ack *p = (Ack *)msg;
				cout << "ACK val: " << p->val << " senderid: " << p->senderid << endl;
				break;
			}
			case DOCMT: {
				Docommit *p = (Docommit *)msg;
				cout << "DOCMT val: " << p->val << " senderid: " << p->senderid << endl;
				break;
			}
			case HAVECMTED: {
				HaveCommitted *p = (HaveCommitted *)msg;
				cout << "HAVECMTED val: " << p->val << " senderid: " << p->senderid << endl;
				break;
			}
			case ABORT: {
				Abort *p = (Abort *)msg;
				cout << "ABORT val: " << p->val << " senderid: " << p->senderid << endl;
				break;
			}
			case ABORTACK: {
				AbortAck *p = (AbortAck *)msg;
				cout << "ABORTACK val: " << p->val << " senderid: " << p->senderid << endl;
				break;

			}
			case ALIVE: {
				Election *p = (Election *)msg;
				cout<< "ALIVE val: " << p->val << " senderid: " << p->senderid << endl;
				break;
			}
			case VICTORY: {
				Election *p = (Election *)msg;
				cout<< "VICTORY val: " << p->val << " senderid: " << p->senderid << endl;
				break;
			}
			case INQUIRY: {
				Election *p = (Election *)msg;
				cout<< "INQUIRY val: " << p->val << " senderid: " << p->senderid << endl;
				break;
			}
			case TRANSIT: {
				Election *p = (Election *)msg;
				cout<< "TRANSIT val: " << p->val << " senderid: " << p->senderid << endl;
				break;
			}
			default: {
				cout << "msg error " << endl;
				exit(1);
			}
		}

	}
	void doFinal(int ctype, int val) {
		if (ctype == A) {
			cout << "id :" << myid << " msg val:" << val << " Abort" << endl;
		} else if (ctype == C) {
			cout << "id :" << myid << " msg val:" << val << " Commit" << endl;
		} else {
			cout << "final type error" << endl;
			exit(1);
		}
	}
	void deleteMsg(int type, char *msg) {
		switch (type) {
			case CANCMT: {
				Cancmt *p = (Cancmt *)msg;
				delete p;
				break;
			}
			case REPLY: {
				Reply *p = (Reply *)msg;
				delete p;
				break;
			}
			case PRECMT: {
				PreCommit *p = (PreCommit *)msg;
				delete p;
				break;
			}
			case ACK: {
				Ack *p = (Ack *)msg;
				delete p;
				break;
			}
			case DOCMT: {
				Docommit *p = (Docommit *)msg;
				delete p;
				break;
			}
			case HAVECMTED: {
				HaveCommitted *p = (HaveCommitted *)msg;
				delete p;
				break;
			}
			case ABORT: {
				Abort *p = (Abort *)msg;
				delete p;
				break;
			}
			case ABORTACK: {
				AbortAck *p = (AbortAck *)msg;
				delete p;
				break;
			}
			case ALIVE: case VICTORY: case INQUIRY: {
				Election *p = (Election *)msg;
				delete p;
				break;
			}
			case TRANSIT: {
				Termination *p = (Termination *)msg;
				delete p;
				break;
			}
			default: {
				cout << "msg error can not delete " << endl;
				exit(1);
			}
		}
	}
};
#endif
