#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void doit(int fd);
void read_requesthdrs(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
void echo(int connfd);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char client_hostname[MAXLINE], client_port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

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
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);
    doit(connfd);  // 트랜잭션 수행
    Close(connfd); // 연결을 닫는다.
  }
  printf("%s", user_agent_hdr);
  return 0;
}

void doit(int fd)
{
  struct stat sbuf;
  int clientfd;
  char buf[MAXLINE], buf2[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], port[MAXLINE], path[MAXLINE], srcp[MAX_CACHE_SIZE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t server_rio, client_rio;
  // rio는 rio_t라는 별도의 구조체인데, 해당 구조체에는 fd정보와 내부적인 임시 버퍼에 대한 정보들도 존재한다.

  Rio_readinitb(&server_rio, fd); // 한 개의 빈 파일구조체 rio와 파일 식별자 fd와 연결한다. 초기화
  Rio_readlineb(&server_rio, buf, MAXLINE);
  // rio_readlineb의 래핑함수 - rio_readlineib : 텍스트 줄을 파일 rio에서 읽고 이것을 문자열 메모리 buf로 복사하고 textline을 NULL문자로 종료시킨다.
  // 위 코드가 종료되는 조건은 파라미터로 입력된 MAXLINE까지 읽거나, 읽는 도중 EOF가 발생하였거나, 혹은 한줄 입력의 경우 개행문자를 만난경우가 해당한다.
  printf("Request headers:\n");
  printf("%s", buf);                             // 문자열 buf print 한다.
  sscanf(buf, "%s %s %s", method, uri, version); // 문자열 buff를 method, uri, version으로 분리하여 저장한다.
  strcpy(version, "HTTP/1.0");

  if (strcasecmp(method, "GET")) // 대소문자를 무시하고 각각의 문자열을 비교한다. GET메소드의 여부를 확인한다.
  // 2개의 문자열이 같으면 0이 출력됨 다르면 음수 혹은 양수가 나옴
  {
    clienterror(fd, method, "501", "NOT implemented", "Tiny does not implement this method");
    return;
  }
  get_host(uri, port, path);
  clientfd = Open_clientfd(uri, port);

  sprintf(buf2, "%s /%s %s\r\n", method, path, version);
  sprintf(buf2, "%sHost: %s\r\n", buf2, uri);
  sprintf(buf2, "%s%s", buf2, user_agent_hdr);
  sprintf(buf2, "%sConnection: close\r\n", buf2);
  sprintf(buf2, "%sProxy-Connection: close\r\n", buf2);
  sprintf(buf2, "%s\r\n", buf2);
  Rio_writen(clientfd, buf2, MAXLINE);

  Rio_readinitb(&client_rio, clientfd);
  Rio_readnb(&client_rio, srcp, MAX_CACHE_SIZE);
  Close(clientfd);
  Rio_writen(fd, srcp, MAX_CACHE_SIZE);
}

void get_host(char *uri, char *port, char *path)
{
  char *p1, *p2, *p3, *p4;
  p1 = strstr(uri, "http://");
  if (p1)
  {
    strcpy(uri, p1 + 7);
  }
  p2 = strstr(uri, "/");
  if (p2)
  {
    strcpy(path, p2 + 1);
  }
  p3 = strstr(uri, "/");
  if (p3)
  {
    strcpy(p3, "");
  }
  p4 = strstr(uri, ":");
  if (p4)
  {
    strcpy(port, p4 + 1);
    strcpy(p4, "");
  }
}

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
