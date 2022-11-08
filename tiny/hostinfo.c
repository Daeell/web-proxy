#include "csapp.h"

int main(int argc, char **argv)
{
    struct addrinfo *p, *listp, hints;
    char buff[MAXLINE]; // 최대 text라인 길이인 8912를 MAXLINE이라는 매크로 값으로 할당한거임
    int rc, flags;

    if (argc != 2)
    {
        fprintf(stderr, "usage : %s <domain name>\n", argv[0]);
        exit(0);
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    // getaddrinfo에서 hint 인자를 넣을 때, int형 4개 멤버에만 값을 지정해준다.
    // ai_flags, ai_family, ai_sockttype, ai_protocol
    // 나머지는 값이 0 혹은 NULL이어야하므로, memset함수를 통해 전체 구조체를 0으로 설정하고 4개이하의 필드에만 값을 지정해준다.
    hints.ai_family = AF_INET;       // 주소체계 - IPv4의 소켓주소를 가진 소켓 주소체만 반환 받을 것이다.
    hints.ai_socktype = SOCK_STREAM; // socket유형 - 신뢰성 있는 연결지향형 socket만 인자로 할 것.
    if ((rc = getaddrinfo(argv[1], NULL, &hints, &listp)) != 0)
    { // getaddrinfo는 성공하면 0 실패하면 errorcode를 반환한다.
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
        exit(1);
    }

    flags = NI_NUMERICHOST; // getnameinfo flag - 숫자주소 문자열을 대신 리턴하게 된다.
    // 그래서 여러가지 값들을 or연산이 가능하다.
    // netdb.h 파일을 보면 flag인자는 비트 마스킹을 위해 1, 2, 4순으로 배치되어 있다.
    for (p = listp; p; p = p->ai_next)
    {
        Getnameinfo(p->ai_addr, p->ai_addrlen, buff, MAXLINE, NULL, 0, flags);
        // 소켓 주소, 소켓 주소의 크기 , 호스트, 호스트의 크기, 서버, 서버의 크기, 플래그
        printf("%s\n", buff);
    }

    freeaddrinfo(listp);
    exit(0);
}