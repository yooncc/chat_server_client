#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define TCP_PORT 5100

void registClient();

int main(int argc, char **argv)
{
    int ssock,pfd[2];
    struct sockaddr_in servaddr;
    char mesg[BUFSIZ];
    char rmesg[BUFSIZ+23];
    char name[20];
    char pw[20];

    if(argc < 2) {
        printf("Usage : %s IP_ADRESS\n", argv[0]);
        return -1;
    }

    pipe(pfd);  // 자식프로세스 종료됨 전달
    
    // 무한 루프
    while(1) {
        printf("**************************************\n");
        printf("*********  Start Client Menu  ********\n");
        printf("******** 1.    Login          ********\n");
        printf("******** 2.    Sign up        ********\n");
        printf("******** 3.    Help           ********\n");
        printf("******** else) Quit           ********\n");
        printf("**************************************\n");
        int index;
        scanf("%d",&index);
        getchar();
        if (index == 1) {   // Login
            // 그냥 진행
        }
        else if (index == 2) {  // Sign up
            registClient();
            continue;
        }
        else if (index == 3) {  // Help
             printf("TCP를 이용한 채팅 프로그램 작성\n"
           "서버와 클라이언트 모델\n"
           "서버와 클라이언트 모두 프로그래밍\n"
           "서버는 멀티 프로세스(fork())를 사용\n"
           "부모 프로세스와 자식 프로세스들 사이에 IPC(pipe 만) 사용\n"
           "채팅 서버는 데몬으로 등록\n"
           "구현할 기능:\n"
           "채팅 서버(라즈베리 파이) / 클라이언트(우분투) 구현\n"
           "클라이언트와 서버는 소켓으로 통신\n"
           "채팅방 기능: 로그인 / 로그 아웃 기능\n"
           "채팅 시 로그인된 ID 뒤에 메시지 표시\n"
           "빌드 시스템은 make/cmake를 이용\n"
           "제약 사항:\n"
           "select() / epoll() 함수 사용 불가\n"
           "메시지 큐나 공유 메모리 사용 불가\n");
           continue;
        }
        else {                  // Quit
            exit(0);
        }

        /* 소켓을 생성 */
        if((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket()");
            return -1;
        }

        /* 소켓이 접속할 주소 지정 */
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;

        /* 문자열을 네트워크 주소로 변경 */
        inet_pton(AF_INET, argv[1], &(servaddr.sin_addr.s_addr));
        servaddr.sin_port = htons(TCP_PORT);

        /* 지정한 주소로 접속 */
        if(connect(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            perror("connect()");
            return -1;
        }

        /***************** 채팅창 입장 **********************/
        printf("enter the ID : ");
        fgets(name, 20, stdin);
        name[strlen(name)-1]='\0';
        printf("enter the PW : ");
        fgets(pw, 20, stdin);
        pw[strlen(pw)-1]='\0';
        
        // 로그인 시작
        sprintf(mesg, "login:%s;%s", name,pw); 
        if (send(ssock,mesg,strlen(mesg),MSG_DONTWAIT) <= 0) {    // 데이터를 소켓에 씀
            perror("send");
            return -1;
        }
        memset(mesg,0,BUFSIZ);
        if (recv(ssock,mesg,BUFSIZ,0) < 0) {
            perror("recv");
            return -1;
        }
        
        if (strcmp(mesg,"loginok")!=0) {    // 실패시 소켓 끊고 다시 로그인으로
            close(ssock); 
            printf("login fail!\n");
            continue;
        }
        
        // 로그인 성공
        sprintf(mesg, " - %s - enter the chatting!", name); 
        if (send(ssock,mesg,strlen(mesg),MSG_DONTWAIT) <= 0) {    // 데이터를 소켓에 씀
            perror("send");
            return -1;
        }
        memset(mesg,0,BUFSIZ);
        printf("******** enter the chatting success ********\n");
        /******************************************************/

        int recvChild,sendChild;
        if ((recvChild = fork()) < 0) {
            perror("fork");
            exit(1);
        }
        else if (recvChild==0) {    // child
            while(1) {
                int n;
                memset(mesg,0,BUFSIZ);
                if ((n=recv(ssock,mesg,BUFSIZ,0)) < 0) {
                    perror("recv");
                    return -1;
                }
                else if (n==0) {
                    // break;
                }
                printf("%s\n",mesg); // 받은 문자열을 화면에 출력
            }
            exit(0);
        }

        if ((sendChild = fork()) < 0) {
            perror("fork");
            exit(1);
        }
        else if (sendChild==0) {
            while(1) {
                memset(mesg,0,BUFSIZ);
                fgets(mesg,BUFSIZ,stdin);
                mesg[strlen(mesg)-1]='\0';
                if (strcmp(mesg,"quit")==0) {
                    sprintf(mesg, "******** -%s- left the chatting ********", name); 
                    if (send(ssock,mesg,strlen(mesg),MSG_DONTWAIT) <= 0) {    // 데이터를 소켓에 씀
                        perror("send");
                        return -1;
                    }
                    break;
                }
                memset(rmesg,0,BUFSIZ+23);
                sprintf(rmesg,"%s : %s",name,mesg);
                if (send(ssock,rmesg,strlen(rmesg),MSG_DONTWAIT) <= 0) {    // 데이터를 소켓에 씀
                    perror("send");
                    return -1;
                }
            }
            // 읽기프로세스 죽이기, 본인(쓰기) 종료
            // kill(recvChild,9);
            exit(0);
        }
        waitpid(sendChild,NULL, 0);
        kill(recvChild,9);
        waitpid(recvChild,NULL, 0);
        // while (waitpid(0,NULL, WNOHANG)==0) {
        //     printf("%d %d\n",recvChild, sendChild);
        //     sleep(1);
        // }
        // kill(recvChild,9);

        close(ssock); 					/* 소켓을 닫는다. */ 
        
        printf("******** Successfully logged out ********\n");
    }
    return 0;
}

void registClient() {
    char id[20], pw[20], *buf;

    FILE *fp;
    
    printf("start sign up (if you want back step press 'q')>> \n");
    
    while(1) {
        printf("enter the ID to be used(within 20 characters,No spaces) >> ");
        scanf("%s",id);
        if (strcmp(id,"q")==0) {
            break;
        }    
        char buffer[256]; 
        int idCheck=0;   

        fp = fopen("clients.txt", "a+");
        if (fp == NULL) {
            printf("파일을 열 수 없습니다.\n");
            exit(-1);
        }
        // 파일에서 한 줄씩 읽기
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            buffer[strlen(buffer)-1]='\0';
            buf = strtok(buffer,";");
            if (strcmp(buf,id)==0) {
                idCheck = 1;
                break;
            }
        }
        fclose(fp);
        if (idCheck == 1) {
            printf("already exist id\n");
            continue;
        }

        printf("enter the PW to be used(within 20 characters,No spaces) >> ");
        scanf("%s",pw);
        if (strcmp(id,"q")==0) {
            break;
        }  
        memset(buffer,0,256);
        sprintf(buffer,"%s;%s\n",id,pw);
        fp = fopen("clients.txt", "a+");
        if (fp == NULL) {
            printf("파일을 열 수 없습니다.\n");
            exit(-1);
        }
        fwrite(buffer,1,strlen(buffer),fp);
        fclose(fp);

        printf("Sign up SUCCESS -ID: %s, PW: %s\n",id,pw);
        break;
         
    }
    

}

