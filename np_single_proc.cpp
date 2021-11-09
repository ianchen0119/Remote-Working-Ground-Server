#include "np_single_proc.h"

using namespace std;

#define empty_ 0
// >
#define redirOut_ 1
// <n
#define redirIn_ 2
// |
#define pipe_ 3
// |n
#define num_pipe1_ 4
// !n
#define num_pipe2_ 5
// >n
#define userPipe_ 6

const int FD_NULL = open("/dev/null", O_RDWR);

vector <userPipe> userPipeVector;
sh instanceArr[30];
fd_set activefds, readfds;
int servingID = 0;

int findMax(int msock){
	int maxValue = msock;
	for (int i = 0; i < 30; i++){
		if (instanceArr[i].ssock > maxValue){
			maxValue = instanceArr[i].ssock;
		}
	}
	return maxValue + 1;
}

void sig_chld(int signo){
    int status;
    wait(&status);
    return;
}

int main(int argc, char *argv[]){
    void sig_chld(int);
    signal(SIGCHLD, sig_chld);

    FD_ZERO(&activefds);
	FD_ZERO(&readfds);

    if (argc != 2){
		return 0;
    }

    for(int i = 0; i < 30; i++){
        instanceArr[i] = sh(i);
    }
    
    unsigned short port = (unsigned short)atoi(argv[1]);

	int msock = create_socket(port);
	listen(msock, 30);

	struct timeval timeval = {0, 10};

	while(true){

		bcopy(&activefds, &readfds, sizeof(readfds));

		if (select(findMax(msock), &readfds, NULL, NULL, &timeval) < 0){
			cerr << "select fail" << endl;
		}

		if (FD_ISSET(msock, &readfds)){
			if(new_user(msock) < 0){
#ifdef DEBUG
		    cerr << "Server is busy..." << endl;
#endif
            }
		}

		for(int i = 0; i < 30; i++){
            instanceArr[i].run();
        }
	}
  
  
    return 0;
}

int new_user(int msock){
    for(int i = 0; i < 30; i++){
        if(!instanceArr[i].isAllocated){
            struct sockaddr_in sin;
            unsigned int alen = sizeof(sin);
            int ssock = accept(msock, (struct sockaddr *)&sin, &alen);
            instanceArr[i].allocate(ssock, sin);

            FD_SET(instanceArr[i].ssock, &activefds);

            welcome(i);
            login(i);

            return 0;
        }
    }

	return -1;
}

void sh::allocate(int ssock_, struct sockaddr_in sin_){
    this->ssock = ssock_;
    this->isAllocated = true;
    this->isDone = true;
    this->address = inet_ntoa(sin_.sin_addr);
    this->address += ":" + to_string(htons(sin_.sin_port));
    envVal new_envVal;
    new_envVal.name = "PATH";
    new_envVal.value = "bin:.";
    this->envVals.push_back(new_envVal);

    for(int k = 0; k < MAX_NUMPIPE; k++){
        this->timerArr[k] = -1;
    }
}

int create_socket(unsigned short port){
    int msock;
	if ((msock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
#ifdef DEBUG
		cerr << "Socket create fail.\n";
#endif
		exit(0);
	}
	struct sockaddr_in sin;

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

	if (bind(msock, (struct sockaddr *)&sin, sizeof(sin)) < 0){
#ifdef DEBUG
		cerr << "Socket bind fail.\n";
#endif
		exit(0);
	}
    FD_SET(msock, &activefds);
	return msock;
}

void broadcast(int *sou, int *des, string msg_){
	const char *msg = msg_.c_str();
	char unit;
	if (des == NULL){
		for (int i = 0; i < 30; i++){
			if (instanceArr[i].isAllocated){
				write(instanceArr[i].ssock, msg, sizeof(unit) * msg_.length());
			}
		}
	} else {
		write(instanceArr[*des].ssock, msg, sizeof(unit) * msg_.length());
	}
}

void who(){
	string msg = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
	for (int i = 0; i < 30; i++){
		if (instanceArr[i].isAllocated){
			msg += to_string(instanceArr[i].id + 1) + "\t" + instanceArr[i].name + "\t" + instanceArr[i].address;
			if (instanceArr[i].id == servingID){
				msg += "\t<-me";
			}
			msg += "\n";
		}
	}
	broadcast(NULL, &servingID, msg);
}

void tell(int targetID, string msg){
	if (instanceArr[targetID].isAllocated){
		msg = "*** " + instanceArr[servingID].name + " told you ***: " + msg + "\n";
		broadcast(NULL, &targetID, msg);
	} else {
		msg = "*** Error: user #" + to_string(targetID + 1) + " does not exist yet. ***\n";
		broadcast(NULL, &servingID, msg);
	}
}

void yell(string msg){
	msg = "*** " + instanceArr[servingID].name + " yelled ***: " + msg + "\n";
	broadcast(NULL, NULL, msg);
}

void setName(string newName){
	for (int i = 0; i < 30; i++){
		if (i == servingID){
			continue;
		}
		if (instanceArr[i].isAllocated && instanceArr[i].name == newName){
			string msg = "*** User '" + newName + "' already exists. ***\n";
			broadcast(NULL, &servingID, msg);
			return;
		}
	}
	instanceArr[servingID].name = newName;
	string msg = "*** User from " + instanceArr[servingID].address + " is named '" + instanceArr[servingID].name + "'. ***\n";
	broadcast(NULL, NULL, msg);
}

void welcome(int id){
	string msg = "";
	msg = msg + "****************************************\n"
			  + "** Welcome to the information server. **\n"
			  + "****************************************\n";
	broadcast(NULL, &id, msg);
}

void login(int id){
	string msg = "*** User '" + instanceArr[id].name + "' entered from " + instanceArr[id].address + ". ***\n";
	broadcast(NULL, NULL, msg);
}

void logout(int id){
	string msg = "*** User '" + instanceArr[id].name + "' left. ***\n";
	broadcast(NULL, NULL, msg);
}

int createUserPipe(int sou, int des, string msg_){
    int index;
    int res;
    if(des < 0 || des > 29 || !instanceArr[des].isAllocated){
        string msg = "*** Error: user #" + to_string(des + 1) + " does not exist yet. ***\n";
        broadcast(NULL, &sou, msg);
        res = -1;
    }else{
        if(checkUserPipe(index, sou, des)){
            string msg = "*** Error: the pipe #" + to_string(sou + 1) + "->#" + to_string(des + 1) + " already exists. ***\n";
            broadcast(NULL, &sou, msg);
            res = -1;
        }else{
            string msg = "*** " + instanceArr[sou].name + " (#" + to_string(sou + 1) + ") just piped '" + msg_ + "' to "
							+ instanceArr[des].name + " (#" + to_string(des + 1) + ") ***\n";
            broadcast(NULL, NULL, msg);
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

int receiveFromUserPipe(int sou, int des, string msg_){
    int index;
    if(sou < 0 || sou > 29 || !instanceArr[sou].isAllocated){
        string msg = "*** Error: user #" + to_string(sou + 1) + " does not exist yet. ***\n";
        broadcast(NULL, &des, msg);
        return -1;
    }else{
        if(checkUserPipe(index, sou, des)){
            string msg = "*** " + instanceArr[des].name + " (#" + to_string(des + 1) + ") just received from "
							+ instanceArr[sou].name + " (#" + to_string(sou + 1) + ") by '" + msg_ + "' ***\n";
			broadcast(NULL, NULL, msg);
        }else{
            string msg = "*** Error: the pipe #" + to_string(sou + 1) + "->#" + to_string(des + 1) + " does not exist yet. ***\n";
			broadcast(NULL, &des, msg);
            return -1;
        }
    }
    return 0;
}

bool checkUserPipe(int &index, int sou, int des){
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

void sh::prompt(){
    if(this->isDone){
        /* Data reset */
        this->cmdBlockCount = 1;
        string msg = "% ";
        broadcast(NULL, &this->id, msg);
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
            if(receiveFromUserPipe(this->cmdBlockSet[i].fd_in, this->id, this->pipeMsg[0]) < 0){
                this->cmdBlockSet[i].fd_in = -2;
            }
        }

        if(this->cmdBlockSet[i].fd_out > -1){
            if(createUserPipe(this->id, this->cmdBlockSet[i].fd_out, this->pipeMsg[1]) < 0){
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
            }else{
                dup2(instanceArr[this->id].ssock, STDOUT_FILENO);
            }

            if(this->cmdBlockSet[i].fd_in < -1){
                dup2(FD_NULL, STDIN_FILENO);
            }
            
			dup2(instanceArr[this->id].ssock, STDERR_FILENO);

            close(instanceArr[this->id].ssock);

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

void sh::run(){

    servingID = this->id;

    if(!this->isAllocated){  
        return;
    }

    clearenv();

    for (int i = 0; i < (int) this->envVals.size(); i++){
		setenv(this->envVals[i].name.c_str(), this->envVals[i].value.c_str(), 1);
	}

    string input;
    char readbuf[15000];

    while(true){
        this->prompt();
        if (FD_ISSET(this->ssock, &readfds)){
            bzero(readbuf, sizeof(readbuf));
            int readCount = read(this->ssock, readbuf, sizeof(readbuf));
            if (readCount < 0){
                cerr << this->id << ": read error." << endl;
                return;
            } else {
                input = readbuf;
                if (input[input.length()-1] == '\n'){
                    input = input.substr(0, input.length()-1);
                    if (input[input.length()-1] == '\r'){
                        input = input.substr(0, input.length()-1);
                    }
                }
            }
        } else {
            return;
        }

        if(input.length() == 0){
            this->isDone = true;
            return;
        }

        this->parser(input, 0, input.length());
            
        if(this->parse[0] == "exit" || this->parse[0] == "EOF"){
            logout(this->id);
            FD_CLR(this->ssock, &activefds);
            close(this->ssock);
            this->parse.clear();
            input.clear();
            this->isDone = true;
            this->isAllocated = false;
            this->name = "(no name)";
            for (int i = 0; i < (int) userPipeVector.size(); i++){
                if (userPipeVector[i].sourceID == this->id || userPipeVector[i].targetID == this->id){
                    close(userPipeVector[i].fd[1]);
                    close(userPipeVector[i].fd[0]);
                    userPipeVector.erase(userPipeVector.begin() + i);
                }
            }
            return;
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
			    broadcast(NULL, &servingID, msg + "\n");
            trigger = true;
        }else if(this->parse[0] == "who"){
            who();
            trigger = true;
        }else if(this->parse[0] == "tell"){
            string msg;
            for(int i = 2; i < (int) this->parse.size(); i++){
                msg += this->parse[i];
                if(i != (int) this->parse.size() - 1){
                    msg += " ";
                }
            }
            tell(stoi(this->parse[1]) - 1, msg);
            trigger = true;
        }else if(this->parse[0] == "yell"){
            string msg;
            for(int i = 1; i < (int) this->parse.size(); i++){
                msg += this->parse[i];
                if(i != (int) this->parse.size() - 1){
                    msg += " ";
                }
            }
            yell(msg);
            trigger = true;
        }else if(this->parse[0] == "name"){
            setName(this->parse[1]);
            trigger = true;
        }

        if(trigger){
            this->parse.clear();
            for(int k = 0; k < MAX_NUMPIPE; k++){
                this->timerArr[k] = (this->timerArr[k] == -1)?(-1):(this->timerArr[k] - 1);
            }
            this->isDone = true;
            return;
        }

        this->parse.clear();
        this->cmdBlockGen(input);
        this->execCmd(input);
        input.clear();
        this->isDone = true;
        return;
    }
}