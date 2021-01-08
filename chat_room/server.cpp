//
// Created by nanyu_net2 on 2021/1/8.
//

#define _GNU_SOURCE 1

#include <arpa/inet.h>
#include <fcntl.h>
#include <fmt/core.h>
#include <cassert>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

using namespace std;


#define USER_LIMIT 5        // user limit
#define BUFFER_SIZE 64      // buffer size
#define FD_LIMIT 65535      // limit for fd

// client info
// 1. ip adresss    2. buf that ready to write to client    3. buf from client
struct client_data
{
    sockaddr_in address;
    char* write_buf;
    char buf[BUFFER_SIZE];
};

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        fmt::print("usage: {} ip_adress port_number", basename(argv[0]));
        return 1;
    }

    // init ip and port
    const char* ip = argv[1];   //  ip adress
    int port = atoi(argv[2]);

    // init address
    int ret =0;
    struct sockaddr_in address{};
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;   // IPV4
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    // init socket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= -1);

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    // init users array, each socketFD corresponds to a one user in users
    auto* users = new client_data[FD_LIMIT];
    pollfd fds[USER_LIMIT + 1];
    int user_conter = 0;
    for (int i=1 ; i<=USER_LIMIT; i++)
    {
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    // listen
    while(true)
    {
        ret = poll(fds, user_conter+1 , -1);
        if(ret<0)
        {
            fmt::print("poll fail\n");
            break;
        }

        for(int i=0;i<user_conter+1; ++i)
        {
            if((fds[i].fd == listenfd)&& (fds[i].revents & POLLIN))
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                if(connfd < 0)
                {
                    fmt::print("errno is : {} \n", errno);
                    continue;
                }
                if(user_conter >= USER_LIMIT)
                {
                    const char* info = "too many users\n";
                    fmt::print("{}", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }

                user_conter++;
                users[connfd].address = client_address;
                setnonblocking(connfd);
                fds[user_conter].fd = connfd;
                fds[user_conter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_conter].revents = 0;
                fmt::print("comes a new user, now have {} users \n",user_conter);
            }
            else if(fds[i].revents & POLLERR)
            {
                fmt::print("get an error from %d\n", fds[i].fd);
                char errors[100];
                memset(errors, '\0', 100);
                socklen_t length = sizeof(errors);
                if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length)<0)
                {
                    fmt::print("get socket option fail\n");
                }
                continue;
            }
            else if(fds[i].revents & POLLRDHUP)
            {
                // if the client close connection, the user should be deleted
                users[fds[i].fd] = users[fds[user_conter].fd];
                close(fds[i].fd);
                fds[i] = fds[user_conter];
                i--;
                user_conter -- ;
                printf("a clinet left\n");
            }
            else if(fds[i].revents & POLLIN)
            {
                int connfd = fds[i].fd;
                memset(users[connfd].buf, '\0', BUFFER_SIZE);
                ret = recv(connfd, users[connfd].buf, BUFFER_SIZE-1,0);
                fmt::print("get {} bytes of client data:{} . From {} \n",ret, users[connfd].buf,connfd);
                if(ret < 0)
                {
                    // if error in read operation, close the connection
                    if(errno != EAGAIN)
                    {
                        close(connfd);
                        users[fds[i].fd] = users[fds[user_conter].fd];
                        fds[i] = fds[user_conter];
                        i--;
                        user_conter--;
                    }
                }
                else if(ret == 0)
                {}
                else
                {
                    // if received client data
                    for( int j=1; j<=user_conter; ++j)
                    {
                        if(fds[j].fd == connfd)
                        {
                            continue;
                        }
                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[connfd].buf;
                    }
                }
            }
            else if(fds[i].revents & POLLOUT)
            {
                int connfd = fds[i].fd;
                if( !users[connfd].write_buf)
                {
                    continue;
                }
                ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf),0);
                users[connfd].write_buf = NULL;

                fds[i].events |= ~POLLIN;
                fds[i].events = POLLIN;
            }

        }
    }
    delete [] users;
    close(listenfd);
    return 0;

}