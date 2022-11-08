/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 서버소켓, listening socket을 open한다.
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept // 연결요청을 접수 - 연결 소켓 생성
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // 트랜잭션 수행
    Close(connfd); // 연결을 닫는다.
  }
}

void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  // rio는 rio_t라는 별도의 구조체인데, 해당 구조체에는 fd정보와 내부적인 임시 버퍼에 대한 정보들도 존재한다.

  Rio_readinitb(&rio, fd); // 한 개의 빈 파일구조체 rio와 파일 식별자 fd와 연결한다. 초기화
  Rio_readlineb(&rio, buf, MAXLINE);
  // rio_readlineb의 래핑함수 - rio_readlineib : 텍스트 줄을 파일 rio에서 읽고 이것을 문자열 메모리 buf로 복사하고 textline을 NULL문자로 종료시킨다.
  // 위 코드가 종료되는 조건은 파라미터로 입력된 MAXLINE까지 읽거나, 읽는 도중 EOF가 발생하였거나, 혹은 한줄 입력의 경우 개행문자를 만난경우가 해당한다.
  printf("Request headers:\n");
  printf("%s", buf);                             // 문자열 buf print 한다.
  sscanf(buf, "%s %s %s", method, uri, version); // 문자열 buff를 method, uri, version으로 분리하여 저장한다.
  if (strcasecmp(method, "GET"))                 // 대소문자를 무시하고 각각의 문자열을 비교한다. GET메소드의 여부를 확인한다.
  // 2개의 문자열이 같으면 0이 출력됨 다르면 음수 혹은 양수가 나옴
  {
    clienterror(fd, method, "501", "NOT implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  is_static = parse_uri(uri, filename, cgiargs); // uri를 파일 이름과 cgi 인자 문자열(비어있을 수도 있음)으로 분석
  // 해당 요청이 정적 혹은 동적 컨텐츠를 위한 것인지 나타내는 flag를 설정한다.
  if (stat(filename, &sbuf) < 0) // 파일이 디스크 상에 있지 않으면 즉시 error message를 보낸다.
  // stat함수를 이용하면 파일의 상태를 알아올 수 있다. 첫번째 인자로 주어진 filename의 상태를 얻어와서 두번째 인자인 sbuf에 채워넣는다. 성공할 경우 0 실패했을 경우 -1을 반환한다.
  {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) // 요청이 정적 컨텐츠를 위한 것이라면
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) // 이 파일이 regular file인지 읽기 권한이 사용자에게 있는지 확인한다.
    {
      // sbuf의 파일 형식이 regular file인지 확인하는 용도이다.
      // #define	S_ISREG(mode)	 __S_ISTYPE((mode), __S_IFREG) 해당 파일형식이 __S_IFREG와 일치하는지 확인하는 매크로임
      // >> define	__S_IFREG	0100000 라는 매크로임 - regular파일을 나타내는 비트 매크로임.
      // >> #define	__S_ISTYPE(mode, mask)	(((mode) & __S_IFMT) == (mask)) 매크로 연산을 통해 해당 파일형식에 해당되는지 확인할 수 있음.
      // #define	__S_IFMT	0170000 라는 매크로가 있음 비트연산을 통해 일치여부를 확인할 수 있는듯

      // S_IRUSR -> 실행권한이 사용자 자신에게 있는지 확인하는 매크로이다. __S_IEXEC 0100이라는 매크로로 확인하는듯.
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
    }
    serve_static(fd, filename, sbuf.st_size); // 정적 컨텐츠를 클라이언트에 제공한다.
  }
  else // 해당 요청이 동적컨텐츠에 관련된 것이라면
  {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) // 실행가능한지 검증하고
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

// clienterror함수 : server의 오류를 체크하고 client에 보고하기 위한 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXLINE];
  // sprint -  body 문자열에 해당 문자열을 저장한다.
  // http reponse body를 구성한다.
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s,<body bgcolor ="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // rio_writen(파일 디스크립터, 쓰기 할 데이터 버퍼, 버퍼 크기)
  // 파일 디스크립터에 해당 크기의 바이트만큼 데이터버퍼만큼 쓰겠다는 뜻.
  // 쓰기 수행시 언제나 버퍼 크기만큼 쓰기 동작을 수행한다. -> 에러 발생시 -1 리턴값은 버퍼크기
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

// read_requesthdrs -> tiny는 요청 헤더내의 어떤 정보도 사용하지 않으며 해당 함수를 호출해서 이를 읽고 무시함.
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  // strcmp(str1, str2) : str1 == str2 -> return 0; str1 < str2 -> return - ; str1 > str2 -> return+
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf); //
  }
  return;
}

// 실행파일의 홈 디렉토리를 cgi-bin이라고 가정할 때, cig-bin을 포함하는 모든 URI는 동적 컨텐츠
int parse_uri(char *uri, char *filename, char *cgiargs) // 동적 컨텐츠라면 1을 리턴하고 아니라면 0을 리턴한다.
{
  char *ptr;

  // char *strstr(const char *string1, const char *string2); string1에서 string2가 시작되는 포인터를 반환한다. string2가 string1에 없다면 NULL을 반환한다.
  if (!strstr(uri, "cgi-bin")) // uri에 cgi-bin이 없다면 정적 콘텐츠 - static contents
  {
    // char *strcpy(char *string1, const char *string2); string2를 string1에서 지정한 위치로 복사한다.
    strcpy(cgiargs, ""); // cgiargs를 지우고
    strcpy(filename, ".");
    // char *strcat(char *string1, const char *string2); string2를 string1에 연결하고 NULL문자로 결과 string을 종료한다.
    strcat(filename, uri);           // 파일이름을 ./index.html과 같은 상대리눅스 경로 이름으로 변환한다.
    if (uri[strlen(uri) - 1] == '/') // URI가 '/'로 끝난다면
      strcat(filename, "home.html"); // 기본 파일 이름을 추가한다.
    return 1;
  }
  else
  { // 동적 컨텐츠 - dynamic contents
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1); // 모든 cgi인자를 추출한다.
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // header로부터 client로 응답을 보낸다
  get_filetype(filename, filetype); // 해당 파일의 filetype을 확인한다.
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  // 요청한 파일의 내용을 fd로 복사해서 body를 client로 보낸다.
  srcfd = Open(filename, O_RDONLY, 0); // filename을 open하고 (읽기전용으로) 식별자 srcfd을 얻는다.
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  srcp = malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  // 요청한 file을 가상 메모리 srcp로 매핑한다.
  // void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
  // start부터 length까지의 메모리 영역을 열린 파일 fd에 대응한다. 대응할 때의 파일 위치는 offset이다.
  // prot - 파일에 대응되는 메모영역의 보호특성을 설정한다. PROT_READ는 읽기 전용이다.
  // flag - 대응되는 객체의 형식을 지정하기 위해 사용한다. MAP_PRIVATE는 프로세스는 대응되는 객체를 다른 프로세스와 공유할 수 없게 한다.
  Close(srcfd);
  // 파일을 메모리에 매핑한 후에 더이상 식별자 srcfd는 필요가 없다. 그래서 해당 파일을 닫는다.
  Rio_writen(fd, srcp, filesize);
  // 파일을 클라이언트에게 전송한다.
  // Munmap(srcp, filesize);
  Free(srcp);
  // 매핑된 가상메모리 주소를 반환한다. - free
}

void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))   // html로 끝난다면 == html 파일이라면
    strcpy(filetype, "text/html"); // filetype에 text/html 문자열을 복사한다.
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "image/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0)
  // Unix 환경에서 fork() 함수는 함수를 호출한 프로세스를 복사하는 기능을 한다.
  // fork() 함수는 프로세스 id, 즉 pid 를 반환하게 되는데 이때 부모 프로세스에서는 자식 pid가 반환되고 자식 프로세스에서는 0이 반환된다. 만약 fork() 함수 실행이 실패하면 -1을 반환한다.
  // 따라서 반환값이 0이라는 의미는 자식 프로세스라는 것
  {
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    // int setenv(const char *name, const char *value, int overwrite);
    // 환경에 name이 존재하지 않으면 name변수에 value를 추가한다 overwrite가 0이면 값을 추가할 수 없다.
    Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */
    // int dup2(int fd, int fd2); fd식별자의 값을 fd2로 바꿔준다.
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
}