#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <netdb.h>  
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define MAX_REQUEST 20000 


int server_read; // 서버에서 읽어온 길이
int totalSize = 0;
int writeSize = 0;
int reqnum = 1;   //요청메세지 몇번째인지
int flag=0;

typedef struct Node //노드 정의
{
    int cacheSize;
    char cache[524288];
    char cacheRq[4096];
    struct Node *next;
}Node;
 
 
typedef struct Queue //Queue 구조체 정의
{
    Node *front; //맨 앞(꺼낼 위치)
    Node *rear; //맨 뒤(보관할 위치)
    int queue_totalSize;//큐의 총 사이즈 
}Queue;



int IsEmpty(Queue *queue)
{
    return queue->queue_totalSize == 0;    //보관 개수가 0이면 빈 상태
}

void InitQueue(Queue *queue)
{
    queue->front = queue->rear = NULL; //front와 rear를 NULL로 설정
    queue->queue_totalSize = 0;//보관 개수를 0으로 설정
}
 
int deleteQ(Queue* queue ,char *cacheRq){
    Node *temp;
    temp = queue->front;

    if(strcmp(temp->cacheRq,cacheRq)==0){
        
        queue->front= temp->next;
         free(temp);
         return 0;
    }

    else if(strcmp(queue->rear->cacheRq,cacheRq)==0){
            return 1;
    }

    else{

        while(temp->next != NULL){

            if(strcmp((temp->next)->cacheRq,cacheRq)==0)
            {
                Node *temp2 = temp->next;
                temp -> next = temp2->next;
                free(temp2);
                return 0;
            }
            temp = temp->next;

        }
    }
    return 1;
}



void Enqueue(Queue *queue, int cacheSize ,char *cache ,char *cacheRq)
{
    Node *newNode = (Node *)malloc(sizeof(Node)); //노드 생성
    newNode->cacheSize = cacheSize;//데이터 설정
    strncpy(newNode->cache , cache , cacheSize);
    strncpy(newNode->cacheRq, cacheRq , 2048);

    newNode->next = NULL;
 
    if (IsEmpty(queue))//큐가 비어있을 때
    {
        queue->front = newNode;//맨 앞을 now로 설정       
    }
    else//비어있지 않을 때
    {
        queue->rear->next = newNode;//맨 뒤의 다음을 now로 설정
    }
    queue->rear = newNode;//맨 뒤를 now로 설정   
    queue->queue_totalSize +=cacheSize;//큐 사이즈 증가
}

 
void Dequeue(Queue *queue)
{
    Node *now;
    if (IsEmpty(queue))//큐가 비었을 때
    {
        perror("Queue Empty");
    }
    now = queue->front;//맨 앞의 노드를 now에 기억
    queue->front = now->next;//맨 앞은 now의 다음 노드로 설정
    free(now);//now 소멸
    
}



char* Search_queue(Queue* queue ,char *cacheRq){ //요청메세지를 넣으면 응답메시지를 리턴 
    Node *temp;
    temp = queue->front;
        
    while(temp != NULL)
    {
        if(strcmp(temp->cacheRq,cacheRq)==0)
        {
            return temp->cache;
        }
        temp = temp->next;
    }

    return cacheRq;
}

int Search_queue_size(Queue* queue ,char *cacheRq){ //요청메세지를 넣으면 응답메시지를 리턴 
    Node *temp;
    temp = queue->front;
    
    while(temp != NULL)
    {
        if(strcmp(temp->cacheRq,cacheRq)==0)
        {
            return temp->cacheSize;
        }
        temp = temp->next;
    }

    return 0;

}



struct request_impo
{
    char method[16];  //GET
    char path[4096];  //주소
    char version[16];  // HTTP1.1
    char page[4096];   //요청대상 
};

void error(char *msg)
{
	perror(msg);
	exit(1);
}

char * pas(char * str) {
	return strchr(strstr(str,"//")+2,'/');
}

void *function(void * argv);
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
Queue queue;

int main(int argc, char *argv[])
{
    int sockfd,clnt_sock;   
    int portno; // 입력받은 포트번호  

    int status;
                   
    socklen_t clilen;
    socklen_t  clnt_addr_size;
     
    struct sockaddr_in pserv_addr, clnt_addr;

    if (argc < 2) {
    	error("ERROR, no port provided");
    }
    //  socket생성 (클라이언트를 위한)
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    	error("ERROR opening socket");
     
    bzero((char *) &pserv_addr, sizeof(pserv_addr)); // 구조체를 초기화 하는 함수 (초기화할 시작주소 , 초기화할 사이즈);
    portno = atoi(argv[1]); //입력받은 프록시서버 포트번호를 저장 
    pserv_addr.sin_family = AF_INET;
    pserv_addr.sin_addr.s_addr = INADDR_ANY; // 32bit ip주소, 프록시서버 ip주소 여기서는 localhost
    pserv_addr.sin_port = htons(portno); 


    //  bind
    if (bind(sockfd, (struct sockaddr *)&pserv_addr, sizeof(pserv_addr)) < 0){ //입력한 주소구조체를 소켓에 넣는다.
    	error("ERROR on binding");
    }	

    // listen    	
    	if(listen(sockfd,5) < 0){  // client가 보낸 정보를 기다림 
     		error("ERROR on listenning");
     	}
    InitQueue(&queue);
   	while(1){
   		pthread_t p_thread;

	 // accept
		clnt_addr_size=sizeof(clnt_addr);   // 주소의 사이즈 저장 
		printf("accept시작\n");
		clnt_sock=accept(sockfd,(struct sockaddr*)&clnt_addr,&clnt_addr_size); // client가 보낸 정보들을 clnt_sock에 담음 
		if(clnt_sock < 0){
			error("ERROR on accept");
		}
		//function(clnt_sock);

		if((status = pthread_create(&p_thread, NULL, &function, (void *) &clnt_sock)) != 0) {
			printf(" thread create error: %s\n", strerror(status));
			exit(0);
		}		
		pthread_detach(p_thread);

	}

	close(sockfd);
	return 0;
}



void *function(void * argv){

		int serv_sockfd,logFd; //소켓 파일디스크립터  
		int clnt_sock = *(int*) argv;
		int serv_portno = 80; //서버 포트번호
		socklen_t  serv_sock_size;

		char sendBuffer[MAX_REQUEST]; // 프록시에서 서버로 보낼 요청을 담은 공간 
        memset(sendBuffer,0,sizeof(sendBuffer));
		char buffer[MAX_REQUEST]; // request내용이 담김
        memset(buffer,0,sizeof(buffer));
		char* hostname;
        hostname=malloc(2048);   
        memset(hostname,0,2048);
		char rqst_buffer[MAX_REQUEST];
        memset(rqst_buffer,0,sizeof(rqst_buffer));
    	char* serv_data;//[5242880]; //서버가 응답하는 데이터담을 공간
    	serv_data=(char*)malloc(sizeof(char)*2048);
    	char* temp_cache;//[524288];
    	temp_cache=(char*)malloc(sizeof(char)*5242880);

		struct request_impo* rqst;
		rqst=(struct request_impo*)malloc(sizeof(struct request_impo));
		struct hostent *server; //서버의 ip주소를 포함하여 여러정보가 들어있다.
		struct sockaddr_in serv_addr;

		int n;

		time_t result;
   		result = time(NULL);
    	struct tm* brokentime = localtime(&result);

 		if(clnt_sock<0){
 			error("ERROR on accept");
 		}
 		memset(rqst_buffer,0, sizeof(rqst_buffer));

 		n = read(clnt_sock,rqst_buffer, sizeof(rqst_buffer));
   		if (n <= 0) {
            close(clnt_sock);
            pthread_exit((void *) 0); 
        }
   	
   		printf("----------request정보------------	\n%s\n",rqst_buffer);

     /* 서버로 요청을 보내는 단계 */
     //request메세지에서 host부분만 파싱한다. 
    	char tokken[MAX_REQUEST];
    	memset(tokken,0,sizeof(tokken));
    	memset(hostname,0,2048);

     	strncpy(tokken,rqst_buffer, n); 
     	strtok(tokken, "//");
        hostname=strtok(NULL, "//");
     	//strcat(hostname ,strtok(NULL, "//"));   
        //sprintf(hostname, "%s", strtok(NULL, "//"));
        

     	printf("---------hostname정보----------- \n%s\n",hostname);

    	serv_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //서버로 보낼 소켓생성
    	if (serv_sockfd < 0) 
    		error("ERROR opening socket");

    	printf("server에서 ip 받아오기전 %s\n" ,hostname);
    	server = gethostbyname(hostname); //ip주소를 server변수에 저장 
    	if (server == NULL) {
    	 	error("ERROR, no such host");
    	} 
    	

     	//소켓에 채울 서버주소정보 저장
     	bzero((char *) &serv_addr, sizeof(serv_addr));  
     	serv_addr.sin_family = AF_INET;
     	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length); // 서버의 ip저장 
     	serv_addr.sin_port = htons(serv_portno); 

   		char* serv_ip = inet_ntoa(serv_addr.sin_addr);//in_addr 구조체를 문자열로된 주소로 변환 ,주소 저장
		printf("\n--------응답받을서버 ip주소--------\n%s\n",serv_ip); // 주소 출력
        
    	if (connect(serv_sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) //서버로 연결을 시도한다. 
    		error("ERROR connecting");

   		char token2[MAX_REQUEST];
   		memset(token2,0,sizeof(token2));
    	char *ptr;
    	strncpy(token2,rqst_buffer, MAX_REQUEST); 
    	ptr = strtok(token2," ");
    	strncpy(rqst->method,ptr,16);
    	ptr = strtok(NULL, " ");
    	strncpy(rqst->path,ptr,1024);

    	strncpy(rqst->page,pas(rqst->path),4096);
    	printf("\n page : %s\n", rqst->page);
    	
	    memset(sendBuffer, 0 , MAX_REQUEST);

    	strcat(sendBuffer, rqst->method);
    	strcat(sendBuffer," ");
    	strcat(sendBuffer, rqst->page);
    	strcat(sendBuffer," ");
    	strcat(sendBuffer, "HTTP/1.1");
    	strcat(sendBuffer,"\r\n");
    	strcat(sendBuffer, "Host: ");
    	strcat(sendBuffer, hostname);
    	strcat(sendBuffer, "\r\n");
    	strcat(sendBuffer, "Connection: close");
    	strcat(sendBuffer, "\r\n\r\n");

    	printf("\n %d번째 \n서버에 보낼 요청메세지 : \n%s \n",reqnum,sendBuffer);
               
        reqnum++;
     
    	if(strcmp(sendBuffer,Search_queue(&queue, sendBuffer))==0) // 캐시에 오브젝트가 없으면 sendbuffer를 리턴
        {
        //만약 캐시에 object가 없다면 
    		write(serv_sockfd, sendBuffer, sizeof(sendBuffer)); //서버에 클라이언트요청을 작성해서 보낸다. 

    	   // 서버 응답중..

    		printf("서버 응답중..... \n");

    	   // 서버가 응답한걸 serv_data에 작성한다. 

    		server_read=0;
    		writeSize=0;
    		totalSize=0;
    		memset(temp_cache, 0, 524288);
        	memset(serv_data, 0, 2048);
                   
    		while ((server_read = read(serv_sockfd, serv_data, sizeof(serv_data))) > 0)
    		{

            	if(totalSize<=524288){
                strcat(temp_cache,serv_data); 
               	}
                writeSize = write(clnt_sock, serv_data, server_read);  // write는 쓴 바이트값을 리턴함 
                totalSize += writeSize;
                memset(serv_data, 0, 2048);
    		}
    		close(serv_sockfd);
   
            printf("totalSize: %d\n",totalSize );
            printf("temp_cache: %s\n", temp_cache);

           	if(totalSize<=524288){  //오브젝트 크기가 512KB보다 작을때만 큐에저장

            	while((queue.queue_totalSize+totalSize)>5242880)
                { //만약 캐시용량 5MB를 초과하면

                    queue.queue_totalSize -= queue.front->cacheSize;
                        
                    Dequeue(&queue);//젤 처음에 들어온 캐시를 삭제

                    printf("캐시용량 초과로 인한 오래된 데이터 삭제 완료\n");
                } 



                Enqueue(&queue,totalSize,temp_cache,sendBuffer);
                printf("저장할 캐시 크기%d\n",totalSize);
                printf("프록시 캐시 저장완료!\n");
            }
            else{
                printf("용량이 512KB보다 커서 저장안함\n");
            }

            memset(temp_cache, 0, totalSize);
            memset(serv_data, 0, 2048);

            close(clnt_sock); // 서버에 보낼때        

        }

       	else
       	{
       		printf("캐시에서 찾는중... \n");

            writeSize=0;
            totalSize=0;
          

            memset(temp_cache, 0, 524288);
            //bzero(temp_cache,5242880);
            
            //printf("응답메세지%s\n", Search_queue(&queue,sendBuffer) );
            

            writeSize = write(clnt_sock, Search_queue(&queue,sendBuffer) , Search_queue_size(&queue,sendBuffer));
            totalSize = writeSize;

            
            strncpy(temp_cache,Search_queue(&queue,sendBuffer),Search_queue_size(&queue,sendBuffer));

            
            flag=deleteQ(&queue, sendBuffer);
            printf("flag값 %d\n",flag );
            if(flag==0){
            Enqueue(&queue,totalSize,temp_cache,sendBuffer);
            }
            


            printf("큐 최신화 완료\n");
            close(clnt_sock); // 서버에 보낼때
       	}

       	printf("프록시 총 용량%d\n",queue.queue_totalSize );
       	close(serv_sockfd);

        pthread_mutex_lock(&lock);
		char *returnTime = asctime(brokentime);
    	returnTime[strlen(returnTime)-1] = '\0'; // 마지막 엔터 없애기 

   	 	printf("\nLog는?\n%s %s %s/%d \n",returnTime,serv_ip,rqst->path,totalSize);

    	if((logFd=open("proxy.log", O_CREAT|O_RDWR|O_APPEND,0644))<0)
    		  error("Error logwrite");

    	dprintf(logFd, "%s: %s %s %i\n", returnTime,serv_ip,rqst->path,totalSize);
    	close(logFd);	
		pthread_mutex_unlock(&lock);


        pthread_exit((void *) 0);                
} 





		    
    


