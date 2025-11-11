#include "headsock.h"

// Function declarations
float str_cli(FILE *fp, int sockfd, struct sockaddr *addr, int addrlen, long *len);  // Transmission function
void tv_sub(struct  timeval *out, struct timeval *in); // Calculate the time interval between out and in

int main(int argc, char **argv)
{
    // Socket and network variables
    int sockfd; // Socket file descriptor
    float ti, rt; // ti = transmission time (ms), rt = data rate
    long len; // Total bytes transmitted
    struct sockaddr_in ser_addr; // Server address structure
    char **pptr; // Pointer for iterating through aliases
    struct hostent *sh; // Host entity structure from DNS lookup
    struct in_addr **addrs; // Array of IP addresses
    FILE *fp; // File pointer for the file to send

    // Check command line arguments: program requires hostname as parameter
    if (argc != 2) 
    {
        printf("Parameters do not match\n");
        exit(1);
    }

    // Resolve hostname to IP address using DNS
    sh = gethostbyname(argv[1]);
    if (sh == NULL) 
    {
        printf("Cannot get host name\n");
        exit(1);
    }

    // Extract IP address list from host structure
    addrs = (struct in_addr **)sh->h_addr_list;
    
    // Display host information
    printf("Canonical name: %s\n", sh->h_name); // Print official hostname
    for (pptr=sh->h_aliases; *pptr != NULL; pptr++) // Loop through all aliases
    {
        printf("The aliases name is: %s\n", *pptr);
    }
    switch (sh->h_addrtype) // Check address family type
    {
        case AF_INET:
            printf("AF_INET\n"); // IPv4 address
            break;
        default:
            printf("unknown addrtype\n");
            break;
    }

    // Open file to send
    if ((fp = fopen ("bigfile.bin","rb")) == NULL) 
    {
        printf("File doesn't exit\n");
        exit(0);
    }
    
    // Create UDP socket (SOCK_DGRAM for connectionless UDP)
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        printf("Error in socket creation\n");
        fclose(fp);
        exit(1);
    }

    // Configure server address structure
    ser_addr.sin_family = AF_INET; // IPv4 protocol
    ser_addr.sin_port = htons(MYUDP_PORT); // Server port (convert to network byte order)
    memcpy(&(ser_addr.sin_addr.s_addr), *addrs, sizeof(struct in_addr)); // Copy IP address from DNS result
    bzero(&(ser_addr.sin_zero), 8); // bzero() zeroes specified number of bytes starting from front to back

    // Perform the transmission and receiving using varying-batch-size protocol
    ti = str_cli(fp, sockfd, (struct sockaddr *)&ser_addr, sizeof(struct sockaddr_in), &len);
    
    // Calculate the average transmission rate (bytes per millisecond = Kbytes/s)
    rt = (len/(float)ti);
    
    // Display transmission statistics
    printf("Time(ms) : %.3f, Data sent(byte): %ld\nData rate: %f (Kbytes/s)\n", ti, (long)len, rt);
    
    // Clean up resources
    close(sockfd); // Close socket
    fclose(fp); // Close file
    exit(0);
}

float str_cli(FILE *fp, int sockfd, struct sockaddr *addr, int addrlen, long *len) 
{
	// Buffer and file handling variables
	char *buf; // Pointer to buffer that will hold entire file
	long lsize, ci; // lsize = total file size, ci = current index position in buffer
	
	// Network packet structures
	struct ack_so ack; // Structure to receive acknowledgments from server
    struct pack_so pack_sends; // Structure for data packets (contains header + data)
	
	// Transmission control variables
	int n, slen; // n = bytes sent/received, slen = size of current packet's data
    int batch_size = 1; // Current batch size (will cycle 1 -> 2 -> 3 -> 1)
    int du_in_batch = 0; // Counter: how many DUs sent in current batch
	
	// Timing variables
	float time_inv = 0.0; // Will store transmission time in milliseconds
	struct timeval sendt, recvt; // Timestamps for start and end of transmission
	struct timeval timeout; // Timeout for receiving ACK
	
    socklen_t from_len;
	ci = 0; // Initialize current index to start of file
	
	// Set receive timeout (500ms)
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
		printf("Failed to set receive timeout\n");
	}

    // Determine file size by seeking to end
    fseek(fp , 0 , SEEK_END); // Move file pointer to end
	lsize = ftell(fp); // Get current position (which is the file size)
	rewind(fp); // Move file pointer back to beginning
	
	// Display file and packet information
	printf("The file length is %d bytes\n", (int)lsize);
	printf("the packet length is %d bytes\n",DATALEN);

    // Allocate memory to contain the whole file
	buf = (char *) malloc(lsize + 1);
	if (buf == NULL) 
    {
        exit(2);
    }
    
    // Read entire file into buffer
    fread(buf, 1, lsize, fp); // Read 'lsize' bytes from file into buf
    buf[lsize] = '\0'; // Null-terminate the buffer
    
    // Start timing the transmission
    gettimeofday(&sendt, NULL);
    
    // Main transmission loop
    while (ci <= lsize) {
        // Determine size of this packet's data
        if ((lsize + 1 - ci) <= DATALEN) 
        {
            slen = lsize + 1 - ci; // Last packet: send remaining bytes
        }
        else
        {
            slen = DATALEN; // Regular packet: send full DATALEN bytes
        }

        // Fill packet structure
        pack_sends.num = ci / DATALEN; // Sequence number (which packet this is)
        pack_sends.len = lsize; // Total file length (server needs this)
        memcpy(pack_sends.data, (buf + ci), slen); // Copy data from file buffer to packet

        // Send packet via UDP
        n = sendto(sockfd, &pack_sends, slen + HEADLEN, 0, addr, addrlen);
        if (n == -1) 
        {
            printf("Send error!\n");
            free(buf);
            return -1;
        }

        ci += slen;
        du_in_batch++; // increment DU count in batch

        // check if we complete the batch
        if (du_in_batch >= batch_size) 
        {
            // Retransmission loop with timeout handling
            int retries = 0;
            int max_retries = 5;
            bool ack_received = false;
            
            while (!ack_received && retries < max_retries) 
            {
                // wait for ack
                from_len = addrlen;
                n = recvfrom(sockfd, &ack, sizeof(ack), 0, addr, &from_len);

                if (n == -1) 
                {
                    // Timeout occurred - need to retransmit the entire batch
                    printf("ACK timeout! Retrying batch (attempt %d/%d)...\n", retries + 1, max_retries);
                    
                    // Rewind ci to beginning of current batch
                    long batch_start = ci - (du_in_batch * DATALEN);
                    if (batch_start < 0) batch_start = 0;
                    
                    // Retransmit all packets in the batch
                    for (int i = 0; i < du_in_batch; i++) 
                    {
                        long retrans_ci = batch_start + (i * DATALEN);
                        int retrans_slen;
                        
                        if ((lsize + 1 - retrans_ci) <= DATALEN) 
                        {
                            retrans_slen = lsize + 1 - retrans_ci;
                        }
                        else
                        {
                            retrans_slen = DATALEN;
                        }
                        
                        pack_sends.num = retrans_ci / DATALEN;
                        pack_sends.len = lsize;
                        memcpy(pack_sends.data, (buf + retrans_ci), retrans_slen);
                        
                        n = sendto(sockfd, &pack_sends, retrans_slen + HEADLEN, 0, addr, addrlen);
                        if (n == -1) 
                        {
                            printf("Retransmission send error!\n");
                            free(buf);
                            return -1;
                        }
                        printf("Retransmitted packet seq=%d\n", pack_sends.num);
                    }
                    
                    retries++;
                    continue;
                }
                
                // Check if ACK is valid (cumulative ACK with last received sequence)
                int expected_seq = (ci / DATALEN) - 1;
                if (ack.num == expected_seq && ack.len == 0) 
                {
                    printf("ACK received for batch of %d DU(s), seq=%d\n\n", batch_size, ack.num);
                    ack_received = true;
                } 
                else 
                {
                    printf("Error in acknowledgment: expected seq=%d, got seq=%d\n", expected_seq, ack.num);
                    free(buf);
                    return -1;
                }
            }
            
            if (!ack_received) 
            {
                printf("Max retries exceeded! Transmission failed.\n");
                free(buf);
                return -1;
            }

            // Move to next batch
            du_in_batch = 0;
            batch_size++;
            // Cycle between batch sizes of 1 to 2 to 3 back to 1
            if (batch_size > 3)
            {
                batch_size = 1;
            }
        }
    }

    gettimeofday(&recvt, NULL);
    *len = ci;

    tv_sub(&recvt, &sendt);
    time_inv = (recvt.tv_sec) * 1000.0 + (recvt.tv_usec) / 1000.0;

    free(buf);
    return time_inv;
}

void tv_sub(struct  timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) <0)
	{
		--out ->tv_sec;
		out ->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}