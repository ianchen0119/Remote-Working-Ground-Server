#ifndef SH_H
#define SH_H

#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_NUMPIPE 200
#define MAX_USER 30
#define MAX_MSG_NUM 10
#define MAX_MSG_SIZE 1024

#define SHMKEY ((key_t) 1127)
#define PERM 0666
/* command symbol */
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

using namespace std;

const char base_dir[] = "/net/gcs/110/310551012";

typedef struct cmdBlock{
    /* numbered pipe */
    int num;
    /* prev symbol */
    int prev = 0;
    /* next symbol */
    int next = 0;
    /* start pos */
    int start = 0;
    /* end pos */
    int end = 0;
    /* user pipe */
    int fd_in = -1;
    int fd_out = -1;
} cmdBlock;

typedef struct envVal{
	string name;
	string value;
} envVal;

typedef struct fifo {
    int	fd;
    char name[64 + 1];
}fifo_t;

typedef struct user{
    char msg[MAX_USER][MAX_MSG_NUM][MAX_MSG_SIZE + 1];
    char address[16];
    /* User ID */
    int id;
    int port;
    /* process id (for signal transmission) */
    int pid;
    bool isAllocated;
    char name[21];
    fifo_t fifo[MAX_USER];
}user;

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
        bool isDone = true;
        string pipeMsg[2];
        vector<envVal> envVals;
        int id;
        /* === Functions === */

        /* Show prompt */
        void prompt();
        /* Divide the cmd to cmdBlock(s) */
        void cmdBlockGen(string input);
        /* Parse the cmdBlock */
        void parser(string input, int start, int end);
        /* Exec the command */
        int execCmd(string input);
    public:
        /* receive msg from other users */
        sh() = default;
        ~sh() = default;
        void run(struct sockaddr_in addr);
};

class socketFunction {
    public:
        static socketFunction& getInstance();
        int getMyID(void);
        user* getUserTable(void);
        string getName(string name_);
        void broadcast(int *des, string msg_);
        void who();
        void tell(int targetID, string msg);
        void yell(string msg);
        void setName(string newName);
        int createUserPipe(int des, string msg_, int *outfd);
        int receiveFromUserPipe(int sou, string msg_, int *infd);
        void removeUserPipe(int sou);
        bool checkUserPipe(int &index, int source, int des);
        /* allocate for new user */
        void allocate(struct sockaddr_in sin_);
        void logout(int id);
        /* user id */
        int mID;
        void welcome(int id);
        void login(int id);
    private:
        /* user table */
        user* userTable;
        socketFunction();
        ~socketFunction() = default;
};



/* Shared memory */
void shm_init(void);


int create_socket(unsigned short port);

#endif