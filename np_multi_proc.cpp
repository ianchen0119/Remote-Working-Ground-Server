#include "np_multi_proc.h"
using namespace std;

// ./demo.sh ../310551012_np_project2/np_multi_proc 12330

const int FD_NULL = open("/dev/null", O_RDWR);

vector <userPipe> userPipeVector;

/*
 *  Allocate a shared memory, that be used to store user informations for each user.
 */
void shm_init(void){
    int shmid = 0;
    user *userTable;

    if((shmid = shmget(SHMKEY, MAX_USER * sizeof(user), PERM | IPC_CREAT)) < 0){
        cerr << "server err: shmget failed (errno #" << errno << ")" << endl;
        exit(1);
    }

    /* for memset */
    if((userTable = (user*) shmat(shmid, NULL, 0)) == (user*) -1){
        cerr << "server err: shmat faild" << endl;
    }

    memset((char*) userTable, 0, MAX_USER * sizeof(user));

    shmdt(userTable);
}

void shm_destroy(int sig)
{
	if(sig == SIGCHLD){
		while (waitpid(-1, NULL, WNOHANG) > 0);
	}else if(sig == SIGINT || sig == SIGQUIT || sig == SIGTERM){
		int	shmid;
		if((shmid = shmget(SHMKEY, MAX_USER * sizeof (user), PERM)) < 0){
			cerr << "server err: shmget failed" << endl;
			exit (1);
		}
		if(shmctl(shmid, IPC_RMID, NULL) < 0){
            cerr << "server err: shmctl IPC_RMID failed" << endl;
			exit (1);
		}
		exit (0);
	}
	signal(sig, shm_destroy);
}

int main(int argc, char *argv[]){

    signal (SIGCHLD, shm_destroy);
	signal (SIGINT, shm_destroy);
	signal (SIGQUIT, shm_destroy);
	signal (SIGTERM, shm_destroy);

    sh instance;
    struct sockaddr_in sin;
    unsigned int alen = sizeof(sin);
    int ssock, c_pid;

    if (argc != 2){
		return 0;
    }
    
    unsigned short port = (unsigned short)atoi(argv[1]);
    
    shm_init();

	int msock = create_socket(port);
	listen(msock, MAX_USER);
	while((ssock = accept(msock, (struct sockaddr *)&sin, &alen))){
        switch(c_pid = fork()){
            case 0:
                dup2(ssock, STDIN_FILENO);
                dup2(ssock, STDOUT_FILENO);
                dup2(ssock, STDERR_FILENO);
                close(msock);
                instance.run(sin);
                close(ssock);
                exit(0);
            default:
                close(ssock);
        }
	}
    return 0;
}

socketFunction::socketFunction(){
    int shmid = 0;

    if((shmid = shmget(SHMKEY, MAX_USER * sizeof(user), PERM)) < 0){
        cerr << "server err: shmget failed (errno #" << errno << ")" << endl;
        exit(1);
    }

    if((userTable = (user*) shmat(shmid, NULL, 0)) == (user*) -1){
        cerr << "server err: shmat faild" << endl;
        exit(1);
    }
}

void socketFunction::allocate(struct sockaddr_in sin_){
    for (int i = 0; i < MAX_USER; i++){
        if(!userTable[i].isAllocated){
            userTable[i].isAllocated = true;
            strncpy(userTable[i].address, inet_ntoa(sin_.sin_addr), sizeof(userTable[i].address) - 1);
            userTable[i].address[sizeof(userTable[i].address) - 1] = '\0';
            userTable[i].pid = (int) getpid();
            userTable[i].id = i;
            userTable[i].port = htons(sin_.sin_port);
            mID = i;
            welcome(userTable[i].id);
            login(userTable[i].id);
            break;
        }else{
            continue;
        }
    }
}

int create_socket(unsigned short port){
    int msock;
	if ((msock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		cerr << "Socket create fail.\n";
		exit(0);
	}
	struct sockaddr_in sin;

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

	if (bind(msock, (struct sockaddr *)&sin, sizeof(sin)) < 0){
		cerr << "Socket bind fail.\n";
		exit(0);
	}
	return msock;
}

int socketFunction::getMyID(void){
    return this->mID;
}

user* socketFunction::getUserTable(void){
    return this->userTable;
}

socketFunction& socketFunction::getInstance(){
    static socketFunction instance;
    return instance;
}

string socketFunction::getName(string name_){
    if (name_ == "")
        return "(no name)";

    return name_;
}

void socketFunction::broadcast(int *des, string msg_){
	if (des == NULL){
		for (int i = 0; i < MAX_USER; i++){
			if (userTable[i].isAllocated){
                for(int j = 0; j < MAX_MSG_NUM; j++){
                    if(userTable[i].msg[mID][j][0] == 0){
                        strncpy(userTable[i].msg[mID][j], msg_.c_str(), MAX_MSG_SIZE + 1);
                        kill(userTable[i].pid, SIGUSR1);
                        break;
                    }
                }
			}
		}
	} else {
        if (*des == mID){
            write(STDOUT_FILENO, msg_.c_str(), sizeof(char) * msg_.length());
        }else{
            for (int i = 0; i < MAX_MSG_NUM; i++){
                if(userTable[*des].msg[*des][i][0] == 0){
                    strncpy(userTable[*des].msg[mID][i], msg_.c_str(), MAX_MSG_SIZE + 1);
                    kill(userTable[*des].pid, SIGUSR1);
                    break;
                }
            }
        }
	}
}

void socketFunction::who(){
	string msg = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
	for (int i = 0; i < MAX_USER; i++){
		if (userTable[i].isAllocated){
			msg += to_string(userTable[i].id + 1) + "\t" + getName(userTable[i].name) + "\t" + string(userTable[i].address) + ":" + to_string(userTable[i].port);
			if (userTable[i].id == mID){
				msg += "\t<-me";
			}
			msg += "\n";
		}
	}
	broadcast(&mID, msg);
}

void socketFunction::tell(int targetID, string msg){
	if (userTable[targetID].isAllocated){
		msg = "*** " + getName(userTable[mID].name) + " told you ***: " + msg + "\n";
		broadcast(&targetID, msg);
	} else {
		msg = "*** Error: user #" + to_string(targetID + 1) + " does not exist yet. ***\n";
		broadcast(&mID, msg);
	}
}

void socketFunction::yell(string msg){
	msg = "*** " + getName(userTable[mID].name) + " yelled ***: " + msg + "\n";
	broadcast(NULL, msg);
}

void socketFunction::setName(string newName){
	for (int i = 0; i < 30; i++){
		if (i == mID){
			continue;
		}
		if (userTable[i].isAllocated && userTable[i].name == newName){
			string msg = "*** User '" + newName + "' already exists. ***\n";
			broadcast(&mID, msg);
			return;
		}
	}
    memset(userTable[mID].name, 0, sizeof(userTable[mID].name));
    strncpy(userTable[mID].name, newName.c_str(),newName.length());
	string msg = "*** User from " + string(userTable[mID].address) + ":" + to_string(userTable[mID].port) + " is named '" + newName + "'. ***\n";
	broadcast(NULL, msg);
}

void socketFunction::welcome(int id){
	string msg = "";
	msg = msg + "****************************************\n"
			  + "** Welcome to the information server. **\n"
			  + "****************************************\n";
	broadcast(&id, msg);
}

void socketFunction::login(int id){
	string msg = "*** User '" + getName(userTable[id].name) + "' entered from " + userTable[id].address + ":" + to_string(userTable[id].port) + ". ***\n";
	broadcast(NULL, msg);
}

void socketFunction::logout(int id){
	string msg = "*** User '" + getName(userTable[id].name) + "' left. ***\n";
	broadcast(NULL, msg);
    memset(&userTable[id], 0, sizeof(user));
    close (STDIN_FILENO);
    close (STDOUT_FILENO);
    close (STDERR_FILENO);
    shmdt(userTable);
}

int socketFunction::createUserPipe(int des, string msg_){
    int index;
    int sou = mID;
    int res;
    if(des < 0 || des > 29 || !userTable[des].isAllocated){
        string msg = "*** Error: user #" + to_string(des + 1) + " does not exist yet. ***\n";
        broadcast(&sou, msg);
        res = -1;
    }else{
        if(checkUserPipe(index, sou, des)){
            string msg = "*** Error: the pipe #" + to_string(sou + 1) + "->#" + to_string(des + 1) + " already exists. ***\n";
            broadcast(&sou, msg);
            res = -1;
        }else{
            string msg = "*** " + getName(userTable[sou].name) + " (#" + to_string(sou + 1) + ") just piped '" + msg_ + "' to "
							+ userTable[des].name + " (#" + to_string(des + 1) + ") ***\n";
            broadcast(NULL, msg);
            userPipe newElement;
            newElement.sourceID = sou;
            newElement.targetID = des;
            if(pipe(newElement.fd) < 0){
                cerr << "err" << endl;
                for(;;){

                }
            }
            userPipeVector.push_back(newElement);
            res = 0;
        }
    }
    return res;
}

int socketFunction::receiveFromUserPipe(int sou, string msg_){
    int index;
    int des = mID;
    if(sou < 0 || sou > 29 || !userTable[sou].isAllocated){
        string msg = "*** Error: user #" + to_string(sou + 1) + " does not exist yet. ***\n";
        broadcast(&des, msg);
        return -1;
    }else{
        if(checkUserPipe(index, sou, des)){
            string msg = "*** " + getName(userTable[des].name) + " (#" + to_string(des + 1) + ") just received from "
							+ userTable[sou].name + " (#" + to_string(sou + 1) + ") by '" + msg_ + "' ***\n";
			broadcast(NULL, msg);
        }else{
            string msg = "*** Error: the pipe #" + to_string(sou + 1) + "->#" + to_string(des + 1) + " does not exist yet. ***\n";
			broadcast(&des, msg);
            return -1;
        }
    }
    return 0;
}

bool socketFunction::checkUserPipe(int &index, int sou, int des){
	bool isExist = false;
	for (int i = 0; i < (int) userPipeVector.size(); i++){
		if (userPipeVector[i].sourceID == sou && userPipeVector[i].targetID == des){
			index = i;
			isExist = true;
			break;
		}
	}
	return isExist;
}

void sig_handler(int sig){
    if (sig == SIGUSR1) {
        user *userTable = socketFunction::getInstance().getUserTable();
        int uid = socketFunction::getInstance().getMyID();
		int	i, j;
		for (i = 0; i < MAX_USER; ++i) {
			for (j = 0; j < MAX_MSG_NUM; ++j) {
				if (userTable[uid].msg[i][j][0] != 0) {
                    write(STDOUT_FILENO, userTable[uid].msg[i][j], strlen (userTable[uid].msg[i][j]));
					memset (userTable[uid].msg[i][j], 0, MAX_MSG_SIZE);
				}
			}
		}
	}

	signal(sig, sig_handler);
}

void sh::prompt(){
    if(this->isDone){
        /* Data reset */
        this->cmdBlockCount = 1;
        cout << "% ";
        this->isDone = false;
    }
}

void sh::cmdBlockGen(string input){
    int count = 0;
    int n = 1;
    int prevSymbol = empty_;
    string targetUser;
    while(count < (int) input.length()){
        switch(input[count]){
            case '!':
            case '|':
                this->cmdBlockSet[this->cmdBlockCount - 1].prev = prevSymbol;
                if(49 <= (int)input[count + 1] && (int)input[count + 1] <= 57){
                    prevSymbol = (input[count] == '|')?(num_pipe1_):(num_pipe2_);
                    string countNum;
                    this->cmdBlockSet[this->cmdBlockCount - 1].next = prevSymbol;
                    while(48 <= (int)input[count + n] && (int)input[count + n] <= 57){
                        countNum.push_back(input[count + n]);
                        n++;
                    }
                    int countNumi = (countNum == "")?(1):stoi(countNum);
                    int emptySpace = -1;
                    for(int k = 0; k < MAX_NUMPIPE; k++){
                        if(this->timerArr[k] == -1 && emptySpace == -1){
                            emptySpace = k;
                        }
                        if(this->timerArr[k] != -1 && this->timerArr[k] == countNumi){
                            this->cmdBlockSet[this->cmdBlockCount - 1].num = k;
                            this->timerArr[k] = countNumi;
                            if(this->cmdBlockSet[0].fd_in == -1 || this->cmdBlockCount > 1){
                                this->cmdBlockSet[this->cmdBlockCount - 1].end = count - 1;
                            }
                            return;
                        }
                    }
                    if(emptySpace < 0){
#ifdef DEBUG
                        cerr << "have no buffer to store the info, related to numbered pipe cmd" << endl;
#endif
                        for(;;){

                        } 
                    }
                    this->cmdBlockSet[this->cmdBlockCount - 1].num = emptySpace;
                    if(pipe(this->numPipefds[this->cmdBlockSet[this->cmdBlockCount - 1].num]) < 0){
#ifdef DEBUG
                        cerr << "err! [pipe]" << endl;
#endif
                    }
                    this->timerArr[emptySpace] = countNumi;
                    if(this->cmdBlockSet[0].fd_in == -1 || this->cmdBlockCount > 1){
                        this->cmdBlockSet[this->cmdBlockCount - 1].end = count - 1;
                    }
                    return;
                }else{
                    prevSymbol = pipe_;
                    this->cmdBlockSet[this->cmdBlockCount - 1].next = prevSymbol;
                    if(this->cmdBlockSet[0].fd_in == -1 || this->cmdBlockCount > 1){
                        this->cmdBlockSet[this->cmdBlockCount - 1].end = count - 1;
                    }
                    this->cmdBlockCount++;
                    this->cmdBlockSet[this->cmdBlockCount - 1].start = count + 1;
                }
                break;
            case '>':
                this->cmdBlockSet[this->cmdBlockCount - 1].prev = prevSymbol;
                if(49 <= (int)input[count + 1] && (int)input[count + 1] <= 57){
                    prevSymbol = userPipe_;
                    targetUser.clear();
                    this->cmdBlockSet[this->cmdBlockCount - 1].next = prevSymbol;
                    n = 1;
                    while(48 <= (int)input[count + n] && (int)input[count + n] <= 57){
                        targetUser.push_back(input[count + n]);
                        n++;
                    }
                    if(this->cmdBlockSet[0].fd_in == -1 || this->cmdBlockCount > 1){
                        this->cmdBlockSet[this->cmdBlockCount - 1].end = count - 1;
                    }
                    int targetUserID = (targetUser == "")?(1):(stoi(targetUser) - 1);
                    this->pipeMsg[1] = input.substr(0, input.length());
                    this->cmdBlockSet[this->cmdBlockCount - 1].fd_out = targetUserID;
                    count += n;
                }else{
                    prevSymbol = redirOut_;
                    this->cmdBlockSet[this->cmdBlockCount - 1].next = prevSymbol;
                    if(this->cmdBlockSet[0].fd_in == -1 || this->cmdBlockCount > 1){
                        this->cmdBlockSet[this->cmdBlockCount - 1].end = count - 1;
                    }
                    this->cmdBlockCount++;
                    // next cmdBlock
                    this->cmdBlockSet[this->cmdBlockCount - 1].start = count + 1;
                }
                break;
            case '<':
                if(49 <= (int)input[count + 1] && (int)input[count + 1] <= 57){
                    targetUser.clear();
                    prevSymbol = redirIn_;
                    this->cmdBlockSet[this->cmdBlockCount - 1].next = prevSymbol;
                    n = 1;
                    while(48 <= (int)input[count + n] && (int)input[count + n] <= 57){
                        targetUser.push_back(input[count + n]);
                        n++;
                    }
                    int targetUserID = (targetUser == "")?(1):(stoi(targetUser) - 1);
                    if(this->cmdBlockSet[0].end == 0){
                        this->cmdBlockSet[this->cmdBlockCount - 1].end = count - 1;
                    }
                    this->pipeMsg[0] = input.substr(0, input.length());
                    this->cmdBlockSet[0].fd_in = targetUserID;
                    count += n;
                }
                break;
            default:
                break;
        }
        count++;
    }
    
    if(prevSymbol == pipe_ || prevSymbol == redirOut_ || prevSymbol == empty_){
        this->cmdBlockSet[this->cmdBlockCount - 1].end = count;
        this->cmdBlockSet[this->cmdBlockCount - 1].prev = prevSymbol;
        this->cmdBlockSet[this->cmdBlockCount - 1].next = empty_;
    }
    return;
}

void sh::parser(string input, int start, int end){
    string temp;
    for(int i = start; i < end; i++){
        if(input[i]==' '){
            if(!temp.empty()){
                this->parse.push_back(temp);
                temp.clear();
            }
            continue;
	    }
        temp.push_back(input[i]);
    }
    this->parse.push_back(temp);	
}

int sh::execCmd(string input){
    int i = 0;
    int p = 0;
    if(pipe(this->pipefds[0]) < 0 || pipe(this->pipefds[1]) < 0){
#ifdef DEBUG
        cerr << "err! [0]" << endl;
#endif
    }
    for(; i < this->cmdBlockCount; i++){
        
        this->parser(input, this->cmdBlockSet[i].start, this->cmdBlockSet[i].end);

        int j = 0;
        for(auto val: this->parse){
            this->execArg[j++] = strdup(val.c_str());
        }

        this->execArg[j] = NULL;

        this->parse.clear();

        if(this->cmdBlockSet[i].fd_in > -1){
            if(socketFunction::getInstance().receiveFromUserPipe(this->cmdBlockSet[i].fd_in, this->pipeMsg[0]) < 0){
                this->cmdBlockSet[i].fd_in = -2;
            }
        }

        if(this->cmdBlockSet[i].fd_out > -1){
            if(socketFunction::getInstance().createUserPipe(this->cmdBlockSet[i].fd_out, this->pipeMsg[1]) < 0){
                this->cmdBlockSet[i].fd_out = -2;
            }
        }

redo:
        int pid = fork();

        int status;
        if(pid < 0){
            while(waitpid(-1, &status, WNOHANG) <= 0){
            }
            goto redo;
        }

        if(pid){
            /* pipe */
            if(this->cmdBlockSet[i].prev == pipe_){
                close(this->pipefds[!p][1]);
                close(this->pipefds[!p][0]);
            }

            if(this->cmdBlockSet[i].next == pipe_){
                p = !p;
            }

            /* numbered pipe */
            for(int k = 0; k < MAX_NUMPIPE; k++){
                if(this->timerArr[k] == 0){
                    close(this->numPipefds[k][1]);
                    close(this->numPipefds[k][0]);
                }
            }

            /* user pipe */
            if(this->cmdBlockSet[i].fd_in > -1){
                int index = this->cmdBlockSet[i].fd_in;
                for (int i = 0; i < (int) userPipeVector.size(); i++){
                    if (userPipeVector[i].sourceID == index && userPipeVector[i].targetID == this->id){
                        close(userPipeVector[i].fd[1]);
                        close(userPipeVector[i].fd[0]);
                        userPipeVector.erase(userPipeVector.begin() + i);
                    }
                }
            }
            
            int status;
            int next = (int)this->cmdBlockSet[i].next;
            if(next != 3){
                waitpid(pid, &status, 0);
            }

            if(this->cmdBlockSet[i].prev == pipe_){
                if(pipe(this->pipefds[p]) < 0){
#ifdef DEBUG
                    cerr << "err! [3]" << endl;
#endif
                }
            }
        }else{
            // child

            if(this->cmdBlockSet[i].fd_out < -1){
                dup2(FD_NULL, STDOUT_FILENO);
            }

            if(this->cmdBlockSet[i].fd_in < -1){
                dup2(FD_NULL, STDIN_FILENO);
            }

            if(this->cmdBlockSet[i].next == pipe_ && this->cmdBlockSet[i].prev == pipe_){
                close(this->pipefds[!p][1]);
                dup2(this->pipefds[!p][0], STDIN_FILENO);
                close(this->pipefds[!p][0]);
                close(this->pipefds[p][0]);
                dup2(this->pipefds[p][1] ,STDOUT_FILENO);
                close(this->pipefds[p][1]);
            }

            /* previous symbol is pipe */
            if(this->cmdBlockSet[i].prev == pipe_ && this->cmdBlockSet[i].next != pipe_){
                close(this->pipefds[p][0]);
                close(this->pipefds[p][1]);
                close(this->pipefds[!p][1]);
                dup2(this->pipefds[!p][0], STDIN_FILENO);
                close(this->pipefds[!p][0]);
            }
            /* next symbol is pipe */
            if(this->cmdBlockSet[i].next == pipe_ && this->cmdBlockSet[i].prev != pipe_){
                close(this->pipefds[!p][0]);
                close(this->pipefds[!p][1]);
                close(this->pipefds[p][0]);
                dup2(this->pipefds[p][1] ,STDOUT_FILENO);
                close(this->pipefds[p][1]);
            }

            /* numbered pipe */
            if(this->cmdBlockSet[i].next == num_pipe1_ || this->cmdBlockSet[i].next == num_pipe2_){
                close(this->numPipefds[this->cmdBlockSet[i].num][0]);
                dup2(this->numPipefds[this->cmdBlockSet[i].num][1] ,STDOUT_FILENO);
                if(this->cmdBlockSet[i].next == num_pipe2_){
                    dup2(this->numPipefds[this->cmdBlockSet[i].num][1] ,STDERR_FILENO);
                }
                close(this->numPipefds[this->cmdBlockSet[i].num][1]);
            }

            for(int k = 0; k < MAX_NUMPIPE; k++){
                if(this->timerArr[k] == 0){
                    close(this->numPipefds[k][1]);
                    dup2(this->numPipefds[k][0], STDIN_FILENO);
                    close(this->numPipefds[k][0]);
                }
            }


            /* user pipe */
            if(this->cmdBlockSet[i].fd_out > -1){
                int index = this->cmdBlockSet[i].fd_out;
                for (int i = 0; i < (int) userPipeVector.size(); i++){
                    if (userPipeVector[i].sourceID == this->id && userPipeVector[i].targetID == index){
                        close(userPipeVector[i].fd[0]);
                        dup2(userPipeVector[i].fd[1], STDOUT_FILENO);
                        close(userPipeVector[i].fd[1]);
                        break;
                    }
                }
            }

            if(this->cmdBlockSet[i].fd_in > -1){
                int index = this->cmdBlockSet[i].fd_in;
                for (int i = 0; i < (int) userPipeVector.size(); i++){
                    if (userPipeVector[i].sourceID == index && userPipeVector[i].targetID == this->id){
                        close(userPipeVector[i].fd[1]);
                        dup2(userPipeVector[i].fd[0], STDIN_FILENO);
                        close(userPipeVector[i].fd[0]);
                        break;
                    }
                }
            }

            /* next symbol is redict */
            if(this->cmdBlockSet[i].next == redirOut_){
                close(STDOUT_FILENO);
                this->parser(input, this->cmdBlockSet[i + 1].start, this->cmdBlockSet[i + 1].end);
                char* filePath = (char*)calloc(0, sizeof(char));
                filePath = strdup(this->parse[0].c_str());
                this->parse.clear();
                creat(filePath, 438);
            }

            if(this->cmdBlockSet[i].prev == redirOut_){
                exit(0);
            }

            /* exec */
            if(execvp(this->execArg[0], this->execArg)){
                    cerr << "Unknown command: [" << this->execArg[0] << "]." << endl;
                    exit(0);
            }
        }

    }

    for(int i = 0; i < this->cmdBlockCount; i++){
            this->cmdBlockSet[i].num = 0;
            this->cmdBlockSet[i].prev = 0;
            this->cmdBlockSet[i].next = 0;
            this->cmdBlockSet[i].start = 0;
            this->cmdBlockSet[i].end = 0;
            this->cmdBlockSet[i].fd_in = -1;
            this->cmdBlockSet[i].fd_out = -1;
    }

    for(int k = 0; k < MAX_NUMPIPE; k++){
        this->timerArr[k] = (this->timerArr[k] == -1)?(-1):(this->timerArr[k] - 1);
    }

    return 0;
}

void sh::run(struct sockaddr_in addr){
    signal(SIGUSR1, sig_handler);
    socketFunction::getInstance().allocate(addr);

    this->id = socketFunction::getInstance().getMyID();
    this->isDone = true;
    envVal new_envVal;
    new_envVal.name = "PATH";
    new_envVal.value = "bin:.";
    this->envVals.push_back(new_envVal);

    for(int k = 0; k < MAX_NUMPIPE; k++){
        this->timerArr[k] = -1;
    }

    clearenv();

    for (int i = 0; i < (int) this->envVals.size(); i++){
		setenv(this->envVals[i].name.c_str(), this->envVals[i].value.c_str(), 1);
	}

    string input;

    while(true){
        this->prompt();

        getline(cin, input);

        if (input[input.length()-1] == '\r'){
			input = input.substr(0, input.length() - 1);
		}

        if(input.length() == 0){
            this->isDone = true;
            continue;
        }

        this->parser(input, 0, input.length());
            
        if(this->parse[0] == "exit" || this->parse[0] == "EOF"){
            socketFunction::getInstance().logout(this->id);
            this->parse.clear();
            input.clear();
            this->isDone = true;
            for (int i = 0; i < (int) userPipeVector.size(); i++){
                if (userPipeVector[i].sourceID == this->id || userPipeVector[i].targetID == this->id){
                    close(userPipeVector[i].fd[1]);
                    close(userPipeVector[i].fd[0]);
                    userPipeVector.erase(userPipeVector.begin() + i);
                }
            }
            pid_t wpid;
            int status = 0;
            while ((wpid = wait(&status)) > 0) {
                ;
            }
            exit(0);
        }
            
        bool trigger = false;

        if(this->parse[0] == "setenv"){
            envVal new_envVal;
            new_envVal.name = this->parse[1];
            new_envVal.value = this->parse[2];
            this->envVals.push_back(new_envVal);
            setenv(this->parse[1].c_str(), this->parse[2].c_str(), 1);
            trigger = true;
        }else if(this->parse[0] == "printenv"){
            string msg;
            if (getenv(this->parse[1].c_str()) != NULL)
                msg = getenv(this->parse[1].c_str());
			    socketFunction::getInstance().broadcast(&id, msg + "\n");
            trigger = true;
        }else if(this->parse[0] == "who"){
            socketFunction::getInstance().who();
            trigger = true;
        }else if(this->parse[0] == "tell"){
            string msg;
            for(int i = 2; i < (int) this->parse.size(); i++){
                msg += this->parse[i];
                if(i != (int) this->parse.size() - 1){
                    msg += " ";
                }
            }
            socketFunction::getInstance().tell(stoi(this->parse[1]) - 1, msg);
            trigger = true;
        }else if(this->parse[0] == "yell"){
            string msg;
            for(int i = 1; i < (int) this->parse.size(); i++){
                msg += this->parse[i];
                if(i != (int) this->parse.size() - 1){
                    msg += " ";
                }
            }
            socketFunction::getInstance().yell(msg);
            trigger = true;
        }else if(this->parse[0] == "name"){
            socketFunction::getInstance().setName(this->parse[1]);
            trigger = true;
        }

        if(trigger){
            this->parse.clear();
            for(int k = 0; k < MAX_NUMPIPE; k++){
                this->timerArr[k] = (this->timerArr[k] == -1)?(-1):(this->timerArr[k] - 1);
            }
            this->isDone = true;
            continue;
        }

        this->parse.clear();
        this->cmdBlockGen(input);
        this->execCmd(input);
        input.clear();
        this->isDone = true;
    }
}