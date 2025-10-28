#include "headsock.h"
bool done = false;

void str_ser4(int sockfd);

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in my_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) 
    {
        printf("error in socket");
        exit(1);
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MYUDP_PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY; // contains 32 bit address. INADDR_ANY means it accepts any server IPs. For a specific IP address, use inet_addr("192.168.1.100")
    bzero(&(my_addr.sin_zero), 8);
	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) 
    {          
		printf("error in binding");
		exit(1);
	}
	printf("start receiving\n");
	while (!done) 
    {
		str_ser4(sockfd); 
	}
	close(sockfd);
	exit(0);
}

void str_ser4(int sockfd)
{
    char buf[BUFSIZE];
	FILE *fp;
	char recvs[DATALEN];
    struct sockaddr_in addr;
	struct ack_so ack;
    struct pack_so received_pack;
	int n = 0;
    int len = sizeof(struct sockaddr_in);
	long lseek = 0;
	bool end = false;
    int expecting = 1;
    int count = 0;

    printf("receiving data!\n");

    while (!end)
    {
        while (count < expecting)
        {
            n = recvfrom(sockfd, &received_pack, sizeof(received_pack), 0, (struct sockaddr *) &addr, &len); // recive packet
            if (n == -1)
            {
                printf("error when receiving\n");
                exit(1);
            }
            int remaining = n - HEADLEN;
            int data_len = remaining;
            if (received_pack.data[data_len - 1] == '\0')
            {
                end = true;
                count = 999;
                data_len--;
            }
            memcpy((buf + lseek), received_pack.data, data_len);
            lseek += data_len;
            count += 1;
        }
        switch (expecting) {
            case 1:
                expecting = 2;
                break;
            case 2:
                expecting = 3;
                break;
            case 3:
                expecting = 1;
                break;
            default:
                printf("sumtin wron");
                exit(1);
        }
        count = 0;
        ack.num = 1;
        ack.len = 0;
        if (sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&addr, len) == -1)
        {
            printf("send ack error!\n");
            exit(1);
        }
    }
    fp = fopen("myUDPreceive.txt", "wb");
    if (fp == NULL)
    {
        printf("File not existing\n");
        exit(1);
    }
    fwrite(buf, 1, lseek, fp);
    fclose(fp);
    printf("a file has been successfully received!\nthe total data received is %d bytes\n", (int)lseek);
}