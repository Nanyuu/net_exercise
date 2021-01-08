//
// Created by nanyu_net2 on 2021/1/8.
//

#define _GNU_SOURCE 1   //require POLLRDHUP

#include <fmt/core.h>
#include <arpa/inet.h>
#include<netinet/in.h>
#include <sys/socket.h>
#include <cassert>
#include<unistd.h>
#include<poll.h>
#include <cstring>
#include <fcntl.h>

#define BUFFER_SIZE 64
using namespace std;

int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        fmt::print("usage: {} ip_address portnumber\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];   //ip adress
    int port = atoi(argv[2]);   //port

    // initialize the server address
    struct sockaddr_in server_adress{};
    bzero(&server_adress, sizeof(server_adress));
    server_adress.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &server_adress.sin_addr);
    server_adress.sin_port = htons(port);

    // init Socket
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert( sockfd ==0 );

    if (connect(sockfd, (struct sockaddr*)&server_adress,sizeof(server_adress))<0)
    {
        fmt::print("connection fail");
        close(sockfd);
        return 1;
    }

    // register the fd for istream(0) and sockFd(readable event)
    pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;     // Stream socket peer closed connection, or shut down writing half of connection.
                                            // "POLLRDHUP" Only for Linux
    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe(pipefd);     // create a pipe. pipefd[1](write) pipefd[0](read)
    assert( ret!= -1);

    while(true)
    {
        ret = poll(fds, 2, -1);     // add fds to poll, 1. istream  2. sockfd
        if(ret <0)
        {
            fmt::print("poll fail");
            break;
        }

        if(fds[1].revents & POLLRDHUP)
        {
            fmt::print("server close connection");
            break;
        }
        else if(fds[1].revents & POLLIN)
        {
            memset(read_buf, '\0', BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE-1,0);
            fmt::print("{} \n", read_buf);
        }

        if(fds[0].revents & POLLIN)
        {
            /* zero copy using splice */
            ret = splice(0, NULL, pipefd[1],NULL,32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            ret = splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        }
    }
    close(sockfd);
    return 0;
}

