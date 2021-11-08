#ifndef SH_H
#define SH_H
#define MAX_NUMPIPE 200
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;

typedef struct cmdBlock{
    /* numbered pipe */
    int num;
    /* prev symbol */
    int8_t prev = 0;
    /* next symbol */
    int8_t next = 0;
    /* start pos */
    int start = 0;
    /* end pos */
    int end = 0;
    /* user pipe */
    int fd_in = -1;
    int fd_out = -1;
} cmdBlock;

typedef struct userPipe{
	int sourceID;
	int targetID;
	int fd[2];
} userPipe;

typedef struct envVal{
	string name;
	string value;
} envVal;

int create_socket(unsigned short port);

int new_user(int msock);

class sh{
    private:
        /* Block counter */
        int cmdBlockCount = 1;
        /* Store the result of parsed cmdBlock */
        vector<string> parse;
        char* execArg[50] = {NULL};
        /* Can handle 2000 sub-commands at once */
        cmdBlock cmdBlockSet[2000];
        int pipefds[2][2];
        int numPipefds[MAX_NUMPIPE][2];
        /* For numbered pipe */
        int timerArr[MAX_NUMPIPE] = {0};
        string pipeMsg[2];

        /* === Functions === */

        /* Show prompt */
        void prompt();
        /* Divide the cmd to cmdBlock(s) */
        void cmdBlockGen(string input);
        /* Parse the cmdBlock */
        void parser(string input, int start, int end);
        /* Exec the command */
        int execCmd(string input);

        vector<envVal> envVals;
    public:
        /* allocate for new user */
        void allocate(int ssock, struct sockaddr_in sin_);
        bool isDone = true;
        string address;
        /* User ID */
        int id;
        int ssock = -1; // slave
        bool isAllocated = false;
        string name = "(no name)";
        sh() = default;
        sh(int id_):id(id_){}
        ~sh() = default;
        void run();
};

void broadcast(int *sou, int *des, string msg_);
void who();
void tell(int targetID, string msg);
void yell(string msg);
void setName(string newName);
void welcome(int id);
void login(int id);
void logout(int id);
int createUserPipe(int sou, int des, string msg_);
int receiveFromUserPipe(int sou, int des, string msg_);
bool checkUserPipe(int &index, int source, int des);

#endif