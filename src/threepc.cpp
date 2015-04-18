#include "threepc.h"
//////////////////////////////
// ./commond -t trax.txt(master read) -h hostfile.txt -m masterid -p portnum
//     0     1   2                    3   4           5     6     7    8    
#define TRY 4
#define ALLSIZE 6
void ThreePC::mainLoop() {
	if (myid == masterid) {  // this is master
		// push the commitment to sending queue
		doMaster();
	} else {
		doSlave();
	}
}
//enum State {INIT, WAIT, ABRT, CMT, PREP};
//enum MsgType {CANCMT = 1, REPLY, PRECMT, ACK, DOCMT, HAVECMTED};
void ThreePC::doMaster() {
	//Tag *tag = new Tag(size, myid);  // used to record reply status
	fd_set fdset;
	bool ongoing = false; // every time there is only one outstanding commitment,
	int AllneedtoSend = ALLSIZE;
	int times = TRY;

	struct timeval timeout = { 2, 0 };  // time for time out

	while (true) {
		if (isTermination == false) { // normal 3pc protocol
			if (ongoing == false && state == INIT) { // if sending queue is not empty and no outstanding msg pick one and send to all
				char t = 0;
				cin >> t;
				if (t != 'y') {
					continue;
				}
				tag->setAlltoFalse();
				times = TRY;
				ongoing = true;
				value++;  // set value;
				char *cancomit = mkMsg(CANCMT, value, myid);

				sendAllMsg(CANCMT, cancomit);
				state = WAIT;
				deleteMsg(CANCMT, cancomit);
			}
			FD_ZERO(&fdset);
			FD_SET(sockfd, &fdset);
			select(sockfd + 1, &fdset, NULL, NULL, &timeout);
			if (FD_ISSET(sockfd, &fdset)) {
				char *p = NULL;
				int type = recvMessage(p);
				if (type == REPLY && state == WAIT) {
					Reply *msg = (Reply *) p;
					cout << "msg val " << msg->val << " value1x " << value
							<< endl;
					assert(value == msg->val);
					if (msg->isyes == 1) {  // received a yes from cohort
						int sender = msg->senderid;
						tag->setTrue(sender);
						if (tag->checkAllTrue() && state == WAIT) { // all said yes

							char *precomit = mkMsg(PRECMT, msg->val, myid);
							tag->setAlltoFalse(); // reSet tag to all false except my
							sendAllMsg(PRECMT, precomit);
							state = PREP; // set state to prepare after sending preCommit
							deleteMsg(PRECMT, precomit); // garbage collection
							times = TRY;
						}
					} else if (msg->isyes == 0) {  // received a no from cohort
						tag->setAlltoFalse();
						char *abrt = mkMsg(ABORT, value, myid);
						sendAllMsg(ABORT, abrt);
						state = ABRT;
						deleteMsg(ABORT, abrt);
						times = TRY;
					}
				} else if (type == ACK && state == PREP) {  // in PREP(prepare)

					Ack *msg = (Ack *) p;
					cout << "msg val " << msg->val << " value2x " << value
							<< endl;
					assert(value == msg->val);
					int sender = msg->senderid;
					tag->setTrue(sender);
					if (tag->checkAllTrue() && state == PREP) { // all ack transmit to CMI commit
						char *docomit = mkMsg(DOCMT, msg->val, myid);
						tag->setAlltoFalse();
						sendAllMsg(DOCMT, docomit);
						state = CMT;  // set state to cmt
						deleteMsg(DOCMT, docomit);
						times = TRY;
					}

				} else if (type == HAVECMTED && state == CMT) {
					HaveCommitted *msg = (HaveCommitted *) p;
					cout << "msg val " << msg->val << " value3x " << value
							<< endl;
					assert(value == msg->val);
					int sender = msg->senderid;
					tag->setTrue(sender);
					if (tag->checkAllTrue() && state == CMT) { // all commit state transmit to INIT
					//tag->setAlltoFalse();
						ongoing = false;
						// do commit
						doFinal(C, value);
						state = INIT;
						times = TRY;
					}
				} else if (type == ABORTACK && state == ABRT) {
					AbortAck *msg = (AbortAck *) p;
					cout << "msg val " << msg->val << " value4x " << value
							<< endl;
					assert(value == msg->val);
					int sender = msg->senderid;
					tag->setTrue(sender);
					if (tag->checkAllTrue() && state == ABRT) {
						ongoing = false;
						// do abort
						doFinal(A, value);
						state = INIT;
						times = TRY;
					}
				}
			} else {  // lost some msg
				if (state == WAIT) {
					if (times-- > 0) { // re-transmit cancommit
						int *ids = tag->getUnsetId();
						char *cancomit = mkMsg(CANCMT, value, myid);

						for (int i = 0; i < size; ++i) {
							if (ids[i] != -1) {
								sendMessage(CANCMT, cancomit, ids[i]);
							}
						}
						deleteMsg(CANCMT, cancomit);
					} else {  // time out
						tag->setsitedown();
						char *abrt = mkMsg(ABORT, value, myid);
						tag->setAlltoFalse();
						sendAllMsg(ABORT, abrt);
						state = ABRT;   // to do
						deleteMsg(ABRT, abrt);
						times = TRY;
					}

				} else if (state == PREP) {
					if (times-- > 0) {
						int *ids = tag->getUnsetId();
						char *precomit = mkMsg(PRECMT, value, myid);

						for (int i = 0; i < size; ++i) {
							if (ids[i] != -1) {
								sendMessage(PRECMT, precomit, ids[i]);
							}
						}
						deleteMsg(PRECMT, precomit);
					} else {  // time out
						char *abrt = mkMsg(ABORT, value, myid);
						tag->setsitedown();
						tag->setAlltoFalse();
						sendAllMsg(ABORT, abrt);
						state = ABRT;  // to do
						deleteMsg(ABRT, abrt);
						times = TRY;

					}

				} else if (state == CMT) {
					if (times-- > 0) {
						int *ids = tag->getUnsetId();
						char *docomit = mkMsg(DOCMT, value, myid);

						for (int i = 0; i < size; ++i) {
							if (ids[i] != -1) {
								sendMessage(DOCMT, docomit, ids[i]);
							}
						}
						deleteMsg(DOCMT, docomit);
					} else {  // different
						tag->setsitedown();
						ongoing = false;
						// do commit
						doFinal(C, value);
						state = INIT;
						times = TRY;
					}
				} else if (state == ABRT) {
					if (times-- > 0) {
						int *ids = tag->getUnsetId();
						char *abrt = mkMsg(ABORT, value, myid);
						for (int i = 0; i < size; ++i) {
							if (ids[i] != -1) {
								sendMessage(ABORT, abrt, ids[i]);
							}
						}
						deleteMsg(ABORT, abrt);
					} else {
						tag->setsitedown();
						ongoing = false;
						// do abort
						doFinal(A, value);
						state = INIT;
						times = TRY;
					}
				}
			}
		} else {
			FD_ZERO(&fdset);
			FD_SET(sockfd, &fdset);
			select(sockfd + 1, &fdset, NULL, NULL, &timeout);
			if (FD_ISSET(sockfd, &fdset)) {
				char *p = NULL;
				int type = recvMessage(p);
				Election *msg = (Election *) p;
				int sender = msg->senderid;
				if (type == REPLY && state == WAIT) { // TO-DO
				}
			}
		}
	}
}

void ThreePC::doSlave() {   ////////////////////////////
	fd_set fdset;
	// state = CMT;   // may result in problem
	//----bool strategy[ALLSIZE] = {true, true, false, true, false, false}; // need to be read from a fil
	//int stgyidx = -1;     ///////////-----------
	while (true) {
		struct timeval timeout = { 1, 0 };  // time for time out
		FD_ZERO(&fdset);
		FD_SET(sockfd, &fdset);

		select(sockfd + 1, &fdset, NULL, NULL, &timeout);
		if (FD_ISSET(sockfd, &fdset)) {
			char *p = NULL;
			int type = recvMessage(p);
			//printMsg(R, p);
			if (type == CANCMT
					&& (state == CMT || state == ABRT || state == INIT)) { // new round
				state = INIT;
				//stgyidx++;   ///////////----------
				Cancmt *msg = (Cancmt *) p;
				assert(msg->senderid == masterid);
				//	cout << "value " << value << " msg->val 1x" << msg->val << endl;
				assert(value <= (int )msg->val); // this check may involve problem
				value = msg->val; // update value;
				char *reply = NULL;
				//cout << value << "ssize  " << strategy.size() << endl;
				assert(value < strategy.size()); ////////////++++++++

				if (strategy[value]) {  /////////++++++++++++
					reply = mkMsg(REPLY, value, myid, 1);
				} else {
					reply = mkMsg(REPLY, value, myid, 0);
				}
				sendMessage(REPLY, reply, masterid);
				deleteMsg(REPLY, reply);
				state = WAIT;
			} else if (type == PRECMT && state == WAIT) {

				PreCommit *msg = (PreCommit *) p;
				assert(msg->senderid == masterid);
				//	cout << "value " << value << " msg->val 2x" << msg->val << endl;
				assert(value == msg->val);
				char *ack = mkMsg(ACK, value, myid);
				sendMessage(ACK, ack, masterid);
				deleteMsg(ACK, ack);
				state = PREP;

			} else if (type == DOCMT && state == PREP) {
				Docommit *msg = (Docommit *) p;
				assert(msg->senderid == masterid);
				//	cout << "value " << value << " msg->val 3x" << msg->val << endl;
				assert(value == msg->val);
				char *havecommitted = mkMsg(HAVECMTED, value, myid);
				sendMessage(HAVECMTED, havecommitted, masterid);
				deleteMsg(HAVECMTED, havecommitted);
				doFinal(C, value);
				state = CMT;
			} else if (type == ABORT && state != ABRT) {
				Abort *msg = (Abort *) p;
				assert(msg->senderid == masterid);
				//	cout << "value " << value << " msg->val 4x" << msg->val << endl;
				assert(value == msg->val);
				char *abortack = mkMsg(ABORTACK, value, myid);
				sendMessage(ABORTACK, abortack, masterid);
				deleteMsg(ABORTACK, abortack);
				doFinal(A, value);
				state = ABRT;
			} else if (type == INQUIRY) { // election anouncement
				Election *msg = (Election *) p;
				if (myid > msg->senderid) { // Then we need to send iamalive
					char *alive = mkMsg(ALIVE, 9999, myid);
					sendMessage(ALIVE, alive, msg->senderid);
					deleteMsg(ALIVE, alive);

					// starts new election
					char *inquiry = mkMsg(INQUIRY, 9999, myid);
					sendAllMsg(INQUIRY, inquiry);
					deleteMsg(INQUIRY, inquiry);
					is_candidate = true;
				}
			} else if (type == ALIVE) { // wait for INQUIRY
				is_candidate = false;
				continue; // if no INQUIRY shows up, it goes back to send inquiry
			} else if (type == VICTORY) {
				Election *msg = (Election *) p;
				masterid = msg->senderid;
			} else {
				// reply(type)
				redundentReply(type, value, strategy[value]); ///////////++++++++++++
				cout << "in redundentReply " << endl;
			}
		} else { // time out
			if ((state == WAIT || state == PREP) && is_candidate == false) { // re-election
				char *inquiry = mkMsg(INQUIRY, 9999, myid);
				sendAllMsg(INQUIRY, inquiry);
				deleteMsg(INQUIRY, inquiry);
				is_candidate = true;
			} else if ((state == WAIT || state == PREP)
					&& is_candidate == true) { // announce victory
				char *victory = mkMsg(VICTORY, 9999, myid);
				sendAllMsg(VICTORY, victory);
				deleteMsg(VICTORY, victory);
				masterid = myid;
				// termination protocol
				if (state == WAIT) { // NON-COMMITABLE
					char *non = mkMsg(NONCOMMITTABLE, 9999, myid);
					sendAllMsg(NONCOMMITTABLE, non);
					deleteMsg(NONCOMMITTABLE, non);
				}
				// TO-DO
				//sendAllMsg();
				//doMaster();
			}
		}

	}
}

// ./commond -t trax.txt(master read) -h hostfile.txt -m masterid -p portnum
//     0         2                        4                 6          8    
int main(int argc, char *argv[]) {

	char *strategy = argv[2];
	char *port = argv[8];
	char *hostfile = argv[4];
	char *masteridstr = argv[6];

	int masterid = atoi(masteridstr);

	ThreePC threepc(port, strategy, hostfile, masterid);
	//process p(port, hostfile, count);
	threepc.mainLoop();
	return 0;
}

