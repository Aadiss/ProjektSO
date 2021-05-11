#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <signal.h>

#define SHMSIZE 512
#define N_N 4
#define KEYNB 21
#define KEYNB2 22

int zczytanebajty=0;
char bajty[10];
int pids[3];
int *shmsignal;

union semun 
{
    int val;
    struct semid_ds *buf;
    unsigned short int *array;
    struct seminfo *__buf;
};

struct wspoldzielona
{
	char bufor[512];
	int size;
};

struct msg_buf
{
	long msg_type;
	char msg_text[512];
};


int main(int argc, char*argv[])
{

	//inicjacja semaforów, kluczy i identyfikatorów pamięci współdzielonej 
	key_t semkey;
	int semid;
	key_t shmkey;
	key_t shmkey2;
	int shmid;
	int shmid2;
	int fd;
	int msgid;
	key_t msgkey;
	
	//inicjacja łącza fifo
	char *lacze="nazwane";
	mkfifo(lacze,0666);
	
	//instancja struktór do przechowywania pamięci współdzielonej i kolejki komunikatów
	struct wspoldzielona *shm;
	struct msg_buf msg;
	struct msg_buf msg2;
	
	//inicjacja semaforów oraz pamięci współdzielonej i kolejki komunikatów 
	union semun op;
	semkey=ftok(".",KEYNB);
	semid=semget(semkey,N_N,IPC_CREAT | 0666);
	shmkey=ftok(".",KEYNB);
	shmkey2=ftok(".",KEYNB2);
	msgkey=ftok(".",KEYNB);
	msgid=msgget(msgkey,IPC_CREAT | 0666);
	msg.msg_type=666;
	shmid=shmget(shmkey,sizeof(struct wspoldzielona),IPC_CREAT | 0666);
	shm=(struct wspoldzielona*)shmat(shmid,NULL,0);
	shmid2=shmget(shmkey2,sizeof(int),IPC_CREAT | 0666);
	shmsignal=(int*)shmat(shmid2,NULL,0);
	
	//przypisanie poczatkowych wartosci semaforom
	op.val=0;
	semctl(semid,0,SETVAL,op);
	op.val=1;
	semctl(semid,1,SETVAL,op);
	op.val=0;
	semctl(semid,2,SETVAL,op);
	op.val=1;
	semctl(semid,3,SETVAL,op);
	
	
	struct sembuf read_lock={1,-1,0};
	struct sembuf read_unlock={1,1,0};
	
	struct sembuf write2_lock={2,-1,0};
	struct sembuf write2_unlock={2,1,0};
	
	//fprintf(stderr,"Proces P0: %d\n",getpid());
	
	if ((pids[0]=fork()))
    {
		
		//Proces P1 odczytujący dane z STDIN i zapisujący je do pamięci wspoldzielonej
		do
		{
			semop(semid,&read_lock,1);
			shm->size=read(0, &msg.msg_text, SHMSIZE);
			//Wysłanie wiadomości kolejką komunikatów
			msgsnd(msgid,&msg,sizeof(msg.msg_text),0);
		}while(shm->size>0);
		
		//Odłączenie pamięci wspoldzielonej i semaforów
		semctl(semid, 0, IPC_RMID);
		semctl(semid, 1, IPC_RMID);
		semctl(semid, 2, IPC_RMID);
		semctl(semid, 3, IPC_RMID);
		shmdt(shm);
		shmdt(shmsignal);
		shmctl(shmid, IPC_RMID, NULL);
		shmctl(shmid2, IPC_RMID, NULL);
    }
	//Proces P2 odczytuje dane od procesu P1 i przekazuje do procesu P3
    if((pids[1]=fork()))
    {
		fd = open(lacze, O_WRONLY);
		//fprintf(stderr,"Proces P2: %d\n",pids[1]);
		do 
		{
			if((msgrcv(msgid,&msg2,sizeof(msg.msg_text),666,0))==-1)
			{
				return 1;
			}
			//fprintf(stderr,"Proces P2 %d\n",pids[1]);
			//fprintf(stderr,"%d\n",sizeof(msg2.msg_text));
			write(fd, msg2.msg_text, shm->size);
			zczytanebajty+=shm->size;
			semop(semid, &write2_unlock,1);
		} while(shm->size > 0);
		fprintf(stderr,"%d\n",zczytanebajty);
		msgctl(msgid, IPC_RMID, NULL); 
		shmdt(shm);
		shmdt(shmsignal);
	}
	//Proces P3 odczytuje dane z pamięci współdzielonej i wyrzuca je na STDOUT
    if((pids[2]=fork()))
    {
		fd=open(lacze,O_RDONLY);
		//fprintf(stderr,"Proces P3: %d\n",pids[2]);
		char buf[512];
		do 
		{
			semop(semid, &write2_lock,1);
			//fprintf(stderr,"Proces P3 %d\n",pids[2]);
			read(fd,buf,shm->size);
			write(1, buf, shm->size);
			semop(semid, &read_unlock,1);
		} while(shm->size > 0);
		shmdt(shm);
		shmdt(shmsignal);
		unlink(lacze);
    }
    
    return 0;
}
