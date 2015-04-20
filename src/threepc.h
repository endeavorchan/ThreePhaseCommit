/* Coordinator
 *	The coordinator receives a transaction request. If there is a failure at this point, the coordinator aborts the transaction 
 * 	(i.e. upon recovery, it will consider the transaction aborted). Otherwise, the coordinator sends a canCommit? message to the cohorts 
 *  and moves to the waiting state.
 *
 *	If there is a failure, timeout, or if the coordinator receives a No message in the waiting state, the coordinator aborts the 
 *	transaction and sends an abort message to all cohorts. Otherwise the coordinator will receive Yes messages from all cohorts within
 * 	the time window, so it sends preCommit messages to all cohorts and moves to the prepared state.
 *
 *	If the coordinator succeeds in the prepared state, it will move to the commit state. However if the coordinator times out while 
 * 	waiting for an acknowledgement from a cohort, it will abort the transaction. In the case where all acknowledgements are received, 
 * 	the coordinator moves to the commit state as well.
 *
 * Cohort
 *	The cohort receives a canCommit? message from the coordinator. If the cohort agrees it sends a Yes message to the coordinator and 
 * 	moves to the prepared state. Otherwise it sends a No message and aborts. If there is a failure, it moves to the abort state.
 *
 *	In the prepared state, if the cohort receives an abort message from the coordinator, fails, or times out waiting for a commit, 
 * 	it aborts. If the cohort receives a preCommit message, it sends an ACK message back and awaits a final commit or abort.
 *
 *	If, after a cohort member receives a preCommit message, the coordinator fails or times out, the cohort member goes forward with
 *  the commit.
 *
 *	./commond -t trax.txt(master read) -h hostfile.txt -m masterid -p portnum
 */

#ifndef THREEPC_H
#define THREEPC_H
#define TRY 4
#include "msg.h"

using namespace std;
enum State {
	INIT = 1, WAIT, ABRT, CMT, PREP
};

class ThreePC: public MSG {
	State state;
	int masterid;
	vector<int> strategy;    //++++++
public:
	int value;
	int times;
	// variables used by termination protocol
	bool is_candidate;
	bool isTermination;
	int *termi_status;
	bool transited;
	ThreePC() {
		init();
	}
	ThreePC(char *msgport, char *sfile, char *hostfile, int masterid): MSG(msgport, hostfile) { /////////++++++++
		init();
		this->masterid = masterid;
		string fname = sfile;
		ifstream infile(fname.c_str(),ios::in);
		int asize = 0;
		infile >> asize;

		cout << "All strategy " << endl;
		strategy.push_back(2);
		for (int i = 0; i < asize; ++i) {
			for (int j = 0; j < this->size; ++j) {
				int s;
				infile >> s;
				cout << s << " ";
				if (j == myid) {
					strategy.push_back(s);
				}
			}
			cout << endl;
		}
		cout << "My strategy " << endl;
		for (int i = 1; i < strategy.size(); ++i) {
			cout << strategy[i] << endl;
		}
	}

	void init() {
		value = 0;
		state = INIT;
		times = TRY;
		tag = new Tag(size, myid); // used to record reply status
		isTermination = false;
		is_candidate = false;
		transited = false;
	}

	State uintToState(uint32_t i){
		if (i==1){
			return INIT;
		} else if (i==2){
			return WAIT;
		} else if (i==3){
			return ABRT;
		} else if (i==4){
			return CMT;
		} else if (i==5){
			return PREP;
		}
	}


	void testElection() { // simulate coordinator failure
		if (state == PREP && myid == 1){
			exit(1);
		}
	}
	void redundentReply(int type, int val, bool isyes) {
		int indicate = 0;
		if (isyes)
		indicate = 1;
		char *msg = NULL;
		int sendtype = -1;
		switch(type) {
			case CANCMT: {
				sendtype = REPLY;
				msg = mkMsg(REPLY, val, myid, indicate);
				break;
			}
			case PRECMT: {
				sendtype = ACK;
				msg = mkMsg(ACK, val, myid);
				break;
			}
			case DOCMT: {
				sendtype = HAVECMTED;
				msg = mkMsg(HAVECMTED, val, myid);
				break;

			}
			case ABORT: {
				sendtype = ABORTACK;
				msg = mkMsg(ABORTACK, val, myid);
				break;
			}
			default: {
				cout << "error in redundentReply" << endl;
				exit(1);
			}
		}
		//char *msg = mkMsg(type, value, myid, indicate);
		sendMessage(sendtype, msg, masterid);
		deleteMsg(sendtype, msg);
	}
	void mainLoop();

	void doMaster();

	void doSlave();
};

#endif 

