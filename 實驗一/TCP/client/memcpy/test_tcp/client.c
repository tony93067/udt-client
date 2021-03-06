#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/times.h>
#include <string.h>

#define DIE(x) perror(x),exit(1)
#define PORT 1290
#define SNAME_SIZE 1024
#define BUFFER_SIZE 1500

int main(int argc, char **argv)
{
    printf("111\n");
    static struct sockaddr_in server;
    clock_t old,new;//use for count executing time
    struct tms time_start,time_end;//use for count executing time
    double ticks;
    int sd;
    struct hostent *host;
    char server_name[SNAME_SIZE];
    char buffer[BUFFER_SIZE];
    int recv_size = 0;
    int recv_packet = 0;

    printf("sizeof(buffer): %ld\n",sizeof(buffer));

    if(argc != 2)
    {
        printf("Usage: %s <server_ip>\n",argv[0]);
        exit(1);
    }

    strcpy(server_name,argv[1]);//set server
    
    /* Set up destination address. */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    //host = gethostbyname(server_name);
    server.sin_port = htons(PORT);    
    //memcpy((char*)&server.sin_addr,host->h_addr_list[0],host->h_length);

    sd = socket(AF_INET,SOCK_STREAM,0);
    if(sd < 0)
    {
        DIE("socket");
    }

    /* Connect to the server. */
    if(connect(sd,(struct sockaddr*)&server,sizeof(server)) == -1)
    {
        DIE("connect");
    }

    printf("Start Receiving!\nPacket Data Size: %d\n",(int)sizeof(buffer));

    /*receive packet*/
    //start time
    if((old = times(&time_start)) == -1)
    {
        DIE("times");
    }

    int total_recv_size = 0;
    int fd;
    int len;
    // open file 
    fd = open("output.txt", O_RDWR | O_CREAT | O_TRUNC | O_APPEND, S_IRWXU);
    if (fd == -1) {
        perror("open\n");
        exit(EXIT_FAILURE);
    }
    while(1)
    {
        memset(buffer, '\0', BUFFER_SIZE);
        if((recv_size = recv(sd, buffer,sizeof(buffer),0)) < 0)
        {
            DIE("recv");
        }
        else
        {
            //printf("recv size %d\n", recv_size); 
            if (recv_size > 0)
            {
                //printf("%s %d\n", buffer, recv_size);
                // ????????????
                if(write(fd, buffer, recv_size) == -1)
                {
                    printf("write error\n");
                    exit(1);
                }
                recv_packet++;
                total_recv_size += recv_size;
            }

        }

        if(recv_size == 0)//last packet, break loop
        {
            close(fd);
            break;
        }
    }
    //finish time
    if((new = times(&time_end)) == -1)
    {
        DIE("times error\n");
    }
    /********************************/
    
    /*executing time*/
    ticks=sysconf(_SC_CLK_TCK);
    printf("Run Time: %2.2f\n",(double)(new-old)/ticks);

    printf("Recv Packet: %d\n",recv_packet);
    printf("Recv Size: %d\n", total_recv_size);
    //close connection
    close(sd);

}

