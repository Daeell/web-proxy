### 📌  Tiny server Implementation

- Tiny server는 정적, 동적 콘텐츠를 제공하는 작은 웹서버이다.
- Tiny는 GET 메소드만 지원한다.

- `argc ≠2` 라는 의미는 연결된 포트가 없다는 것을 의미한다. argc는 argv의 개수이다.
- 그럼 왜 1개만 있어도 되는게 아니냐라고 생각할 수 있는데, 자기 자신을 가리키는 주소값이 있기 때문에 자기 자신을 포함하고 2개가 있어야 연결할 포트가 있다는 것을 의미한다.
- `open_listenfd` 를 통해 `server_socket`의 `listen`까지 호출된 `listening socket`이 생성된다.
- `connfd`는 연결용 소켓이다.  `client`로부터 (`clientaddr`) `listenfd`가 연결요청을 받아들일 때 해당 소켓을 생성하여 `client`로부터 연결요청을 받아들인다.

📌 Using Functions
```
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
```

## 📌 Concurrent Programming

- 한 개의 클라이언트 요청만 처리하는 서버를 적절하지 못하다.
- 다수의 네트워크 클라이언트에게 서비스를 수행하기 위해서는 동시성 서버를 생성하여 클라이언트에게 별도의 서비스를 제공하는 방식이 필요하다.
- 구현은 Thread를 사용한 동시성 서버를 구현하였다.
- 새롭게 생성한 피어쓰레드 `tid`가 연결식별자 포인터 `*commfdp`를 역참조하는 것  >> `Pthread_create`
- 쓰레드를 분리하는 이유 : 분리된 쓰레드의 특징 때문  >>`다른 쓰레드에 의해 청소되거나 종료되지 않고 해당 쓰레드가 종료되면 자동으로 메모리 자원을 반환`
- 서버에서 연결 요청을 수신할 때마다 새로운 피어쓰레드가 생성될 수 있다. 이때 각각의 연결이 쓰레드 별로 독립적으로 처리되므로 각각에 피어 쓰레드가 종료되기를 기다릴 필요없이 종료되자마자 메모리자원을 반환하는 방식이 적절하다.

####################################################################
# CS:APP Proxy Lab
#
# Student Source Files
####################################################################

This directory contains the files you will need for the CS:APP Proxy
Lab.

proxy.c
csapp.h
csapp.c
    These are starter files.  csapp.c and csapp.h are described in
    your textbook. 

    You may make any changes you like to these files.  And you may
    create and handin any additional files you like.

    Please use `port-for-user.pl' or 'free-port.sh' to generate
    unique ports for your proxy or tiny server. 

Makefile
    This is the makefile that builds the proxy program.  Type "make"
    to build your solution, or "make clean" followed by "make" for a
    fresh build. 

    Type "make handin" to create the tarfile that you will be handing
    in. You can modify it any way you like. Your instructor will use your
    Makefile to build your proxy from source.

port-for-user.pl
    Generates a random port for a particular user
    usage: ./port-for-user.pl <userID>

free-port.sh
    Handy script that identifies an unused TCP port that you can use
    for your proxy or tiny. 
    usage: ./free-port.sh

driver.sh
    The autograder for Basic, Concurrency, and Cache.        
    usage: ./driver.sh

nop-server.py
     helper for the autograder.         

tiny
    Tiny Web server from the CS:APP text

