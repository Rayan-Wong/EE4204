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
    long total_file_size = 0;
    int expecting_seq = 0;     // Track expected sequence number

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
            
            // Check sequence number for out-of-order detection
            if (received_pack.num != expecting_seq) {
                printf("Out of order! Expected seq %d, got %d - discarding\n", 
                       expecting_seq, received_pack.num);
                continue; // Discard and wait for retransmission
            }
            
            printf("receiving data! seq=%d\n", received_pack.num);
            
            int remaining = n - HEADLEN;
            int data_len = remaining;
            
            // Get total file size from first packet
            if (lseek == 0)
            {
                total_file_size = received_pack.len;
                printf("Expected file size: %ld bytes\n", total_file_size);
            }
            
            // Check for buffer overflow
            if (lseek + data_len > BUFSIZE)
            {
                printf("Error: buffer overflow! Received data exceeds BUFSIZE.\n");
                end = true;
                break;
            }
            
            memcpy((buf + lseek), received_pack.data, data_len);
            lseek += data_len;
            count += 1;
            expecting_seq++; // Increment expected sequence number
            
            // Check if we've received all data
            if (lseek >= total_file_size)
            {
                end = true;
                break;
            }
        }
        
        // Send cumulative ACK after each packet (single DU variant)
        count = 0;
        ack.num = expecting_seq - 1; // Cumulative ACK: last successfully received sequence number
        ack.len = 0;
        if (sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)&addr, len) == -1)
        {
            printf("send ack error!\n");
            exit(1);
        }
        printf("Sent cumulative ACK for seq=%d\n", ack.num);
    }
    fp = fopen("bigfilereceive.bin", "wb");
    if (fp == NULL)
    {
        printf("File not existing\n");
        exit(1);
    }
    fwrite(buf, 1, lseek, fp);
    fclose(fp);
    done = true;  // Signal server to terminate after receiving file
    printf("a file has been successfully received!\nthe total data received is %d bytes\n", (int)lseek);
}