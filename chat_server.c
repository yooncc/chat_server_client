#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>

void disconnectSetting(int*,int,int);
int idPwCheck(char*);

void child_handler(int signo)
{
	waitpid(0, NULL, WNOHANG);
}


#define TCP_PORT 5100 				/* 서버의 포트 번호 */
#define CLIENT_NO 500

int main(int argc, char **argv)
{
    int ssock; 					/* 소켓 디스크립트 정의 */
    socklen_t clen;
    struct sockaddr_in servaddr, cliaddr; 	/* 주소 구조체 정의 */
    char mesg[BUFSIZ],pipeBuf[2],*buf;
	int yes = 1;
    int clients[CLIENT_NO]={0}, client_num=0, pfd[2], pfd2[2], pfd3[2], pfd4[2],tempClients[CLIENT_NO];

	struct sigaction act;
	act.sa_handler = child_handler;
	sigfillset(&act.sa_mask);
	act.sa_flags=SA_RESTART;

	sigaction(SIGCHLD, &act, NULL);
    /* 서버 소켓 생성 */
    if((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        return -1;
    }

	if(setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
   {
       perror("setsockopt");
       exit(1);
   }

	/* 주소 구조체에 주소 지정 */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(TCP_PORT); 	/* 사용할 포트 지정 */

    /* bind 함수를 사용하여 서버 소켓의 주소 설정 */
    if(bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind()");
        return -1;
    }

    /* 동시에 접속하는 클라이언트의 처리를 위한 대기 큐를 설정 */
    if(listen(ssock, 8) < 0) {
        perror("listen()");
        return -1;
    }

    pipe(pfd);  //  protocol
    pipe(pfd2); // clients , mesg
    pipe(pfd3); //  csock
    pipe(pfd4); //  clients update check 
    int flags = fcntl(pfd4[0], F_GETFL,0);
    flags |= O_NONBLOCK;
    fcntl(pfd4[0],F_SETFL,flags);

    int writeChild=-1,readChild;
    while(1) {
        clen = sizeof(cliaddr);
        int csock = accept(ssock, (struct sockaddr *)&cliaddr, &clen);
        inet_ntop(AF_INET, &cliaddr.sin_addr, mesg, BUFSIZ);
        
        // 클라이언트 접속의 변화가 있을 때 논블락으로 읽기
        if (read(pfd4[0],clients,sizeof(clients))>0) {  
            if (clients[0]==0) {
                client_num = 0;
            }
            else {
                for (int i=0;clients[i]!=0;i++) {
                    client_num = i+1;
                } 
            }
        }

        // 클라이언트 접속 성공
        printf("Client is connected : %s\n", mesg);
        clients[client_num] = csock;
        client_num++;
        printf("current client num : %d\nclients sd >> ", client_num);
        for (int i=0;i<client_num;i++) {
            printf("%d ",clients[i]);
        }
        printf("\n");
        strcpy(pipeBuf,"c"); 
        write(pfd[1],pipeBuf,sizeof(pipeBuf));
        write(pfd2[1],clients,sizeof(clients)); 
        
        if ((readChild=fork())<0) {
            perror("fork");
            exit(1);
        }
        else if (readChild == 0) {
            close(ssock);
            while(1) {
                memset(mesg,0,BUFSIZ);
                int n;
                if ((n=read(csock,mesg,sizeof(mesg)))>0) {
                    printf("%s\n",mesg);
                }
                else if (n==0) {
                    printf("disconnect : %d\n",csock);
                    strcpy(pipeBuf,"d"); 
                    write(pfd[1],pipeBuf,sizeof(pipeBuf));
                    write(pfd3[1],&csock,sizeof(int));
                    exit(0);
                }

                // 클라이언트 첫 로그인
                if ((strncmp(mesg,"login:",6) == 0) && (strchr(mesg,';') != NULL)) {
                    buf=strtok(mesg,":");
                    buf=strtok(NULL,"");
                    if (idPwCheck(buf) == 1) {   // 로그인 성공
                        strcpy(buf,"loginok");
                    }
                    else {                  // 로그인 실패
                        strcpy(buf,"loginfail");
                    }
                    write(csock,buf,sizeof(buf)+1);
                    // free(buf);  // 다 썼으면 메모리 초기화
                }
                else {
                    strcpy(pipeBuf,"m"); 
                    write(pfd[1],pipeBuf,sizeof(pipeBuf));
                    write(pfd2[1],mesg,strlen(mesg));
                    write(pfd3[1],&csock,sizeof(int));
                }
            }
        }
        if (writeChild != -1) {
            kill(writeChild,9);
        }

        if ((writeChild=fork())<0) {
            perror("fork");
            exit(1);
        }
        else if (writeChild == 0) {
            while(1) {
                if (read(pfd[0],pipeBuf,sizeof(pipeBuf))>0) {   // 받아오는 프로토콜에 따라 기능 실행
                    if (strcmp(pipeBuf,"c")==0) {
                        read(pfd2[0],clients,sizeof(clients));
                    }
                    else if (strcmp(pipeBuf,"m")==0) {
                        int ccsock;
                        memset(mesg,0,BUFSIZ);
                        read(pfd2[0],mesg,BUFSIZ);
                        read(pfd3[0],&ccsock,sizeof(int));
                        for (int i=0;clients[i]!=0;i++) {
                            if (clients[i]!=ccsock) {
                                if (write(clients[i],mesg,strlen(mesg))<0) {
                                    perror("write");
                                }
                            }
                        }
                    }
                    else if (strcmp(pipeBuf,"d")==0) {
                        int dcsock;
                        read(pfd3[0],&dcsock,sizeof(int));
                        for (int i=0;clients[i]!=0;i++) {
                            if (clients[i] == dcsock) {
                                disconnectSetting(clients,i,CLIENT_NO);
                                break;
                            }
                        }
                        read(pfd4[0],tempClients,sizeof(clients));
                        write(pfd4[1],clients,sizeof(clients));
                    }
                }
            }
        }
    }

    while (waitpid(0,NULL, WNOHANG)==0) {
        sleep(10);
    }

    close(pfd[0]);  close(pfd[1]);
    close(pfd2[0]);  close(pfd2[1]);
    close(pfd3[0]);  close(pfd3[1]);    
    close(ssock); 			/* 서버 소켓을 닫음 */

    return 0;
}

void disconnectSetting(int* arr,int point,int size) {
    for (int i=point;i<size;i++) {
        arr[i] = arr[i+1];
    }   
}

int idPwCheck(char* string) {
    FILE *file;
    file = fopen("clients.txt", "a+");
    if (file == NULL) {
        printf("파일을 열 수 없습니다.\n");
        exit(-1);
    }
    
    char buffer[256]; 
    // 파일에서 한 줄씩 읽기
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        buffer[strlen(buffer)-1]='\0';
        if (strcmp(string,buffer)==0) {
            fclose(file);
            return 1;
        }
    }
    
    // 파일 닫기
    fclose(file);
    return 0;
}