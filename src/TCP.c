#include "TCP.h"
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

boundSocket *TCP_Connect(char *dest_ip_address, int dest_port, int src_port) // NULL on failure
{
    // prepare syn header
    char buffer[TCP_HEADER_LENGTH];
    memset(buffer, 0, TCP_HEADER_LENGTH);
    TCP_Header *header = (TCP_Header *)buffer;
    header->source_port = htons(src_port);
    header->destination_port = htons(dest_port);

    // seq_no must be randomly chosen
    srand(time(NULL));
    header->seq_no = htonl(rand() % 10000); // I'm unsure about how to handle overflows, so I'm limiting this
    header->ack_no = 0;                     // not used
    header->data_offset = TCP_HEADER_WORDS;

    header->syn = 1;

    // open new socket for connection
    socket_t sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0)
    {
        perror("socket()");
        exit(-1);
    }
    // to pass dest info to Network layer
    struct sockaddr_in destination;
    destination.sin_addr.s_addr = inet_addr(dest_ip_address);
    destination.sin_family = AF_INET;

    // start a timer to retry/fail
    uint8_t timer = 0;
    int error = 0;

    // make a buffer to receive the SYN-ACKed packet;
    char recvBuffer[TCP_HEADER_LENGTH];

    // send packet
    while (1)
    {
        if (timer == 8)
        {
            printf("Connection timed out. Please ensure that the destination port is listening.\n");
            return NULL;
        }

        if (error >= 0 && timer % 2 == 0)
            error = sendto(sock, buffer, TCP_HEADER_LENGTH, 0, (struct sockaddr *)&destination, sizeof(struct sockaddr_in));
        if (error < 0)
        {
            perror("sendto() error");
            exit(-1);
        }

        int bytes = recvfrom(sock, recvBuffer, TCP_HEADER_LENGTH, 0, NULL, NULL);

        if (bytes < 0)
        {
            perror("recvfrom()");
            exit(-1);
        }
        if (bytes > 0)
        {
            TCP_Header *recvdHeader = (TCP_Header *)recvBuffer;
            if (recvdHeader->syn && recvdHeader->ack && recvdHeader->ack_no == header->seq_no + TCP_HEADER_LENGTH)
            {
                printf("SYN-ACK response received. ACKing...\n");

                // reusing our previous header,
                header->ack = 1;
                header->syn = 0;
                header->seq_no += TCP_HEADER_LENGTH;
                header->ack_no = recvdHeader->seq_no + TCP_HEADER_LENGTH;

                // send
                if (sendto(sock, buffer, TCP_HEADER_LENGTH, 0, (struct sockaddr *)&destination, sizeof(struct sockaddr_in)) < 0)
                {
                    perror("Last step of 3 way handshake");
                    exit(-1);
                }

                printf("Connection Established. Returning bound socket to caller...\n");
                boundSocket *bs = malloc(sizeof(boundSocket)); // caller calls free
                bs->socket_fd = sock;
                bs->destination = destination;
                return bs;
            }
        }
        sleep(1);
        timer++;
    }
}

boundSocket *TCP_ListenAndAccept(int port) // server connection
{
    socket_t sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    struct sockaddr_in source;
    socklen_t addr_len;
    char buffer[TCP_HEADER_LENGTH];
    char sendbuf[TCP_HEADER_LENGTH];
    memset(buffer, 0, TCP_HEADER_LENGTH);
    memset(sendbuf, 0, TCP_HEADER_LENGTH);
    TCP_Header *header = (TCP_Header *)buffer;
    TCP_Header *sheader = (TCP_Header *)sendbuf;
    int bytes = 0;

    // for seq_no
    srand(time(NULL));

    while (1) // keep listening
    {
        bytes = recvfrom(sock, buffer, TCP_HEADER_LENGTH, 0, (struct sockaddr *)&source, &addr_len);
        if (bytes < 0)
        {
            perror("recvfrom() error");
            exit(-1);
        }
        if (header->syn) // if it is a syn packet
        {
            sheader->source_port = port;
            sheader->destination_port = source.sin_port;
            sheader->seq_no = htonl(rand() % 10000);
            sheader->ack_no = htonl(header->seq_no + TCP_HEADER_LENGTH);
            sheader->data_offset = TCP_HEADER_WORDS;
            sheader->syn = 1;
            sheader->ack = 1;

            if (sendto(sock, sendbuf, TCP_HEADER_LENGTH, 0, (struct sockaddr *)&source, sizeof(struct sockaddr_in)) < 0)
            {
                perror("SYN-ACKing error");
                exit(-1);
            }
            break;
        }
    }

    while (1)
    {
        if (recvfrom(sock, buffer, TCP_HEADER_LENGTH, 0, (struct sockaddr *)&source, &addr_len) < 0)
        {
            perror("ACK recipt error");
            exit(-1);
        }
        if (header->ack && header->ack_no == sheader->seq_no + TCP_HEADER_LENGTH)
        {
            printf("Connection established.\n");
            break;
        }
    }

    boundSocket *bs = malloc(sizeof(boundSocket));
    bs->socket_fd = sock;
    bs->destination = source; // sorry for the naming, but i think it makes sense. destination for all succesive communications.
    return bs;
}

// TCP_Accept // accept first request.

/*TESTING*/
#include <stdio.h>
//
int main(int argc, char const *argv[])
{
    // srand(time(NULL));
    // unsigned int rando = (unsigned int)rand();
    // uint32_t rand2 = (uint32_t)rand();
    // printf("%u %u\n", htonl(rand()), htonl(rand()));
    boundSocket *bs = TCP_Connect("127.0.0.1", 1025, 1024);
    return 0;
}
