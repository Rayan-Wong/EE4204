#include "headsock.h"

int main(int argc, char **argv)
{
    int sockfd, ret;
    float ti, rt;
    long len;
    struct sockaddr_in ser_addr;
    char **pptr;
    struct hostent *sh;
    struct in_addr **addrs;
    FILE *fp;

    if (argc != 2) 
    {
        printf("Parameters do not match");
    }

    sh = gethostbyname(argv[1]);
    if (sh == NULL) 
    {
        printf("Cannot get host name");
        exit(0);
    }

    printf("Canonical name: %s\n", sh->h_name);
    for (pptr=sh->h_aliases; *pptr != NULL; pptr++) 
    {
        printf("The aliases name is: %s\n", *pptr);
    }
    switch (sh->h_addrtype)
    {
        case AF_INET:
            printf("AF_INET\n");
            break;
        default:
            printf("unknown addrtype\n");
            break;
    }
}