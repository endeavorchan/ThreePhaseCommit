#include "threepc.h"
// ./commond -t trax.txt(master read) -h hostfile.txt -m masterid -p portnum
//     0     1   2                    3   4           5     6     7    8    

#define ALLSIZE 6

void ThreePC::mainLoop() {
	if (myid == masterid) {  // this is master
		// push the commitment to sending queue
		doMaster(false);
	} else {
		doSlave();
	}
}

void ThreePC::doMaster(bool ongo) {
	//Tag *tag = new Tag(size, myid);  // used to record reply status
	bool ongoing = ongo; // every time there is only one outstanding commitment,
	while (true) {
		fd_set fdset;
		struct timeval timeout = { 2, 0 };  // time for time out
		if (TESTELECTION1) {
			testElection1();
		} else if (TESTELECTION2) {
			testElection2();
		} else if (TESTSUB) {
			testSubsequentFail();
		}
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
						char *docomit = mkMsg(DOCMT, value, myid);
						tag->setsitedown();
						tag->setAlltoFalse();
						sendAllMsg(DOCMT, docomit);
						state = CMT;  // set state to cmt
						deleteMsg(DOCMT, docomit);
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
			// phase 1
			// sends its state to other sites to make them transit to the state
			if (!transited && state != CMT && state != ABRT) {
				char *transit = mkMsg(TRANSIT, state, myid);
				sendAllMsg(TRANSIT, transit);
				deleteMsg(TRANSIT, transit);
				transited = true;
			}
			if (state == CMT) { // no ACK will be needed
				char *docomit = mkMsg(DOCMT, value, myid);
				sendAllMsg(DOCMT, docomit);
				state = CMT;  // set state to cmt
				deleteMsg(DOCMT, docomit);
				times = TRY;
				isTermination = false;
				continue;
			}
			if (state == ABRT) {
				char *abrt = mkMsg(ABORT, value, myid);
				sendAllMsg(ABORT, abrt);
				state = ABRT;
				deleteMsg(ABORT, abrt);
				times = TRY;
				isTermination = false;
				continue;
			}
			FD_ZERO(&fdset);
			FD_SET(sockfd, &fdset);
			select(sockfd + 1, &fdset, NULL, NULL, &timeout);
			if (FD_ISSET(sockfd, &fdset)) {
				char *p = NULL;
				int type = recvMessage(p);
				Termination *msg = (Termination *) p;
				int sender = msg->senderid;
				tag->setTrue(sender);
				if (type == ACK && tag->checkAllTrue()) { // we can send commit or abort now
					if (state == CMT || state == PREP) {
						char *docomit = mkMsg(DOCMT, value, myid);
						tag->setAlltoFalse();
						sendAllMsg(DOCMT, docomit);
						state = CMT;  // set state to cmt
						deleteMsg(DOCMT, docomit);
						times = TRY;
						isTermination = false;
						continue;
					} else {
						tag->setAlltoFalse();
						char *abrt = mkMsg(ABORT, value, myid);
						sendAllMsg(ABORT, abrt);
						state = ABRT;
						deleteMsg(ABORT, abrt);
						times = TRY;
						isTermination = false;
						continue;
					}
				} else {
				}
			} else {
				if (times-- > 0) {
					int *ids = tag->getUnsetId();
					char *transit = mkMsg(TRANSIT, state, myid);
					for (int i = 0; i < size; i++) {
						if (ids[i] != -1) {
							sendMessage(TRANSIT, transit, ids[i]);
						}
					}
					deleteMsg(TRANSIT, transit);
				} else {
					// time out
					// some slave fails during termination protocol
					tag->setsitedown();
					tag->setAlltoFalse();
					if (state != PREP) {
						char *abrt = mkMsg(ABORT, value, myid);
//						cout<<"1111111111111111111" <<endl;
						sendAllMsg(ABORT, abrt);
						state = ABRT;
						deleteMsg(ABORT, abrt);
					} else {
						char *docomit = mkMsg(DOCMT, value, myid);
//						cout<<"22222222222222222222" << endl;
						sendAllMsg(DOCMT, docomit);
						state = CMT;  // set state to cmt
						deleteMsg(DOCMT, docomit);
					}
					times = TRY;
					isTermination = false;
					cout << "time out in termination" << endl;
					continue;
				}
			}
		}
	}
}

void ThreePC::doSlave() {
	fd_set fdset;
	while (true) {
		struct timeval timeout = { 2, 0 };  // time for time out
		FD_ZERO(&fdset);
		FD_SET(sockfd, &fdset);

		select(sockfd + 1, &fdset, NULL, NULL, &timeout);
		if (FD_ISSET(sockfd, &fdset)) {
			char *p = NULL;
			int type = recvMessage(p);
			if (type == CANCMT
					&& (state == CMT || state == ABRT || state == INIT)) { // new round
				if(TESTSLAVEDOWN1){
					testSlaveDown1(type);
				}
				state = INIT;
				Cancmt *msg = (Cancmt *) p;
				assert(msg->senderid == masterid);
				assert(value <= (int )msg->val); // this check may involve problem
				value = msg->val; // update value;
				char *reply = NULL;
				assert(value < strategy.size());

				if (strategy[value]) {
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
				assert(value == msg->val);
				char *ack = mkMsg(ACK, value, myid);
				sendMessage(ACK, ack, masterid);
				deleteMsg(ACK, ack);
				state = PREP;

			} else if (type == DOCMT && state == PREP) {
				if(TESTSLAVEDOWN2){
					testSlaveDown2(type);
				}
				Docommit *msg = (Docommit *) p;
				assert(msg->senderid == masterid);
				assert(value == msg->val);
				char *havecommitted = mkMsg(HAVECMTED, value, myid);
				sendMessage(HAVECMTED, havecommitted, masterid);
				deleteMsg(HAVECMTED, havecommitted);
				doFinal(C, value);
				state = CMT;
			} else if (type == ABORT && state != ABRT) {
				Abort *msg = (Abort *) p;
				assert(msg->senderid == masterid);
				assert(value == msg->val);
				char *abortack = mkMsg(ABORTACK, value, myid);
				sendMessage(ABORTACK, abortack, masterid);
				deleteMsg(ABORTACK, abortack);
				doFinal(A, value);
				state = ABRT;
			} else if (type == INQUIRY) { // election anouncement
				Election *msg = (Election *) p;
				assert(9999 == msg->val);
				if (myid > msg->senderid) { // Then we need to send iamalive
					char *alive = mkMsg(ALIVE, 9999, myid);
					sendMessage(ALIVE, alive, msg->senderid);
					deleteMsg(ALIVE, alive);

					// starts new election
					char *inquiry = mkMsg(INQUIRY, 9999, myid);
					sendAllMsg(INQUIRY, inquiry);
					deleteMsg(INQUIRY, inquiry);
					is_candidate = true;
				} else {
					is_candidate = false;
				}
			} else if (type == ALIVE) { // wait for INQUIRY
				Election *msg = (Election *) p;
				assert(9999 == msg->val);
				cout << "iamout" << endl;
				is_candidate = false;
			} else if (type == VICTORY) {
				Election *msg = (Election *) p;
				assert(9999 == msg->val);
				masterid = msg->senderid;
				is_candidate = false;
				cout << "new selected master's id is: " << masterid << endl;
				//exit(1);
			} else if (type == TRANSIT) {
				Termination *msg = (Termination *) p;
				char *ack = mkMsg(ACK, value, myid);
				sendMessage(ACK, ack, masterid);
				deleteMsg(ACK, ack);
				state = uintToState(msg->val);
				cout << "new state: " << state << endl;
			} else {
				redundentReply(type, value, strategy[value]);
				cout << "in redundentReply " << endl;
			}
		} else { // time out
			if ((state == WAIT || state == PREP) && is_candidate == false) { // re-election
				if (times-- > 0) {
				} else {
					tag->siteDown(masterid);
					cout << "inquiry" << endl;
					char *inquiry = mkMsg(INQUIRY, 9999, myid);
					sendAllMsg(INQUIRY, inquiry);
					deleteMsg(INQUIRY, inquiry);
					is_candidate = true;
					times = TRY;
				}
			} else if ((state == WAIT || state == PREP)
					&& is_candidate == true) { // announce victory
				cout << "victory" << endl;
				char *victory = mkMsg(VICTORY, 9999, myid);
				sendAllMsg(VICTORY, victory);
				deleteMsg(VICTORY, victory);
				masterid = myid;
				// termination protocol
				isTermination = true;
				doMaster(true);
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
