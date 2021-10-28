#include "np_simple.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

#define empty_ 0
// >
#define redirOut_ 1
// <
#define redirIn_ 2
// |
#define pipe_ 3
// |n
#define num_pipe1_ 4
// !n
#define num_pipe2_ 5

void sig_chld(int signo){
    int status;
    wait(&status);
    return;
}

int main(int argc, char *argv[]){
    void sig_chld(int);
    signal(SIGCHLD, sig_chld);

    sh instance;
    if (argc != 2){
		return 0;
    }
    
    unsigned short port = (unsigned short)atoi(argv[1]);

	int msock = create_socket(port);
	listen(msock, 1);

	int ssock, client_pid;
	while((ssock = new_user(msock))){
		switch(client_pid = fork()){
			case 0:
				dup2(ssock, STDIN_FILENO);
				dup2(ssock, STDOUT_FILENO);
				dup2(ssock, STDERR_FILENO);
				close(msock);

				instance.run();
				break;
			default:
				close(ssock);
				waitpid(client_pid, NULL, 0);
				while(waitpid(-1, NULL, WNOHANG) > 0){}
		}
	}
  
  
    return 0;
}

int new_user(int msock){
	struct sockaddr_in sin;
	unsigned int alen = sizeof(sin);
	int ssock = accept(msock, (struct sockaddr *)&sin, &alen);

	return ssock;
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
	return msock;
}

void sh::prompt(){
    /* Data reset */
    this->cmdBlockCount = 1;
    cout << "% ";
}

void sh::cmdBlockGen(string input){
    int count = 0;
    int n = 1;
    int prevSymbol = empty_;
    this->cmdBlockSet[this->cmdBlockCount - 1].start = count;
    while(input[count] != '\0'){
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
                            this->cmdBlockSet[this->cmdBlockCount - 1].end = count - 1;
                            return;
                        }
                    }
                    if(emptySpace < 0){
#ifdef DEBUG
                        cerr << "have no buffer to store the info, related to numbered pipe cmd" << endl;
                        for(;;){

                        }  
#endif
                    }
                    this->cmdBlockSet[this->cmdBlockCount - 1].num = emptySpace;
                    if(pipe(this->numPipefds[this->cmdBlockSet[this->cmdBlockCount - 1].num]) < 0){
#ifdef DEBUG
                    cerr << "err! [2]" << endl;
#endif
                    }
                    this->timerArr[emptySpace] = countNumi;
                    this->cmdBlockSet[this->cmdBlockCount - 1].end = count - 1;
                    return;
                }else{
                    prevSymbol = pipe_;
                    this->cmdBlockSet[this->cmdBlockCount - 1].next = prevSymbol;
                    this->cmdBlockSet[this->cmdBlockCount - 1].end = count - 1;
                    this->cmdBlockCount++;
                    this->cmdBlockSet[this->cmdBlockCount - 1].start = count + 1;
                }
                break;
            case '>':
                this->cmdBlockSet[this->cmdBlockCount - 1].prev = prevSymbol;
                prevSymbol = redirOut_;
                this->cmdBlockSet[this->cmdBlockCount - 1].next = prevSymbol;
                this->cmdBlockSet[this->cmdBlockCount - 1].end = count - 1;
                this->cmdBlockCount++;
                // next cmdBlock
                this->cmdBlockSet[this->cmdBlockCount - 1].start = count + 1;
                break;
            default:
                break;
        }
        count++;
    }
    this->cmdBlockSet[this->cmdBlockCount - 1].prev = prevSymbol;
    this->cmdBlockSet[this->cmdBlockCount - 1].end = count;
    this->cmdBlockSet[this->cmdBlockCount - 1].next = empty_;
    
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
#ifdef DEBUG
        cout << "Block" << i << ".prev: "  << (int)cmdBlockSet[i].prev << endl;
        cout << "Block" << i << ".next: "  << (int)cmdBlockSet[i].next << endl;
        cout << "Block" << i << ".start: "  << cmdBlockSet[i].start << endl;
        cout << "Block" << i << ".end: "  << cmdBlockSet[i].end << endl;
#endif
        this->parser(input, this->cmdBlockSet[i].start, this->cmdBlockSet[i].end);

        int j = 0;
        for(auto val: this->parse){
            this->execArg[j++] = strdup(val.c_str());
        }

        this->execArg[j] = NULL;

        this->parse.clear();
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

            // /* numbered pipe */
            for(int k = 0; k < MAX_NUMPIPE; k++){
                if(this->timerArr[k] == 0){
                    close(this->numPipefds[k][1]);
                    close(this->numPipefds[k][0]);
                }
            }
            
            int status;
            int next = (int)this->cmdBlockSet[i].next;
            if(next <= 2){
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
            if(this->cmdBlockSet[i].next == pipe_ && this->cmdBlockSet[i].prev == pipe_){
                close(this->pipefds[!p][1]);
                dup2(this->pipefds[!p][0], STDIN_FILENO);
                close(this->pipefds[!p][0]);
                close(this->pipefds[p][0]);
                dup2(this->pipefds[p][1] ,STDOUT_FILENO);
                dup2(this->pipefds[p][1] ,STDERR_FILENO);
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
                dup2(this->pipefds[p][1] ,STDERR_FILENO);
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


            /* next symbol is redict */
            if(this->cmdBlockSet[i].next == redirOut_){
                close(STDOUT_FILENO);
                this->parser(input, this->cmdBlockSet[i+1].start, this->cmdBlockSet[i+1].end);
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

    for(int k = 0; k < MAX_NUMPIPE; k++){
                this->timerArr[k] = (this->timerArr[k] == -1)?(-1):(this->timerArr[k] - 1);
            }

    return 0;
}

void sh::run(){

    string input;

    setenv("PATH", "bin:.", 1);

    for(int k = 0; k < MAX_NUMPIPE; k++){
                this->timerArr[k] = -1;
            }

    while(true){
        this->prompt();

        getline(cin, input, '\n');
        
        this->parser(input, 0, input.length());
        
        if(this->parse[0] == "exit"){
            exit(0);
        }
        
        if(this->parse[0] == "setenv"){
            setenv(this->parse[1].c_str(), this->parse[2].c_str(), 1);
            this->parse.clear();
            for(int k = 0; k < MAX_NUMPIPE; k++){
                this->timerArr[k] = (this->timerArr[k] == -1)?(-1):(this->timerArr[k] - 1);
            }
            continue;
        }else if(this->parse[0] == "printenv"){
            if (getenv(this->parse[1].c_str()) != NULL)
                cout << getenv(this->parse[1].c_str()) << endl;
            this->parse.clear();
            for(int k = 0; k < MAX_NUMPIPE; k++){
                this->timerArr[k] = (this->timerArr[k] == -1)?(-1):(this->timerArr[k] - 1);
            }
            continue;
        }
        
        this->parse.clear();

        if(input.length() == 0){
            continue;
        }

        this->cmdBlockGen(input);
        this->execCmd(input);

        input.clear();
    }
}