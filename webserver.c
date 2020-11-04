/*
 * MIT License
 *
 * Copyright (c) 2018 Lewis Van Winkle
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "chap07.h"


const char *get_content_type(const char* path) { //判斷輸進來的path header是哪種格式
    const char *last_dot = strrchr(path, '.');
    if (last_dot) {
        if (strcmp(last_dot, ".css") == 0) return "text/css";
        if (strcmp(last_dot, ".csv") == 0) return "text/csv";
        if (strcmp(last_dot, ".gif") == 0) return "image/gif";
        if (strcmp(last_dot, ".htm") == 0) return "text/html";
        if (strcmp(last_dot, ".html") == 0) return "text/html";
        if (strcmp(last_dot, ".ico") == 0) return "image/x-icon";
        if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".js") == 0) return "application/javascript";
        if (strcmp(last_dot, ".json") == 0) return "application/json";
        if (strcmp(last_dot, ".png") == 0) return "image/png";
        if (strcmp(last_dot, ".pdf") == 0) return "application/pdf";
        if (strcmp(last_dot, ".svg") == 0) return "image/svg+xml";
        if (strcmp(last_dot, ".txt") == 0) return "text/plain";
    }

    return "application/octet-stream";
}


SOCKET create_socket(const char* host, const char *port) { 
	/*-------創建address------*/
    printf("Configuring local address...\n");
    struct addrinfo hints; //hints = address info
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; //AF_INET 代表 Address Family ,PF_INET 代表 Protocal Family, 但由於兩者值相同,所以混用也沒差
    hints.ai_socktype = SOCK_STREAM; //SOCK_STREAM 代表 傳輸單位為bit,支援TCP protocal
    hints.ai_flags = AI_PASSIVE; // AI_PASSIVE 用於 Server socket 的 bind()

    struct addrinfo *bind_address; // bind_address = 要綁定的address
    getaddrinfo(host, port, &hints, &bind_address); //取得這個address的: (1) host (2) port (3) info (4) address space
	/*-----創建socket------*/
    printf("Creating socket...\n");
    SOCKET socket_listen; 
    socket_listen = socket(bind_address->ai_family,
            bind_address->ai_socktype, bind_address->ai_protocol);  //socket要監聽的address
    if (!ISVALIDSOCKET(socket_listen)) { //看看socket的監聽是否成功
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Binding socket to local address...\n"); //監聽成功,就進行socket bind() address
    if (bind(socket_listen,
                bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) { //listen的queue如果小於10就 fail 
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return socket_listen; //socket_listen即為socket結構
}



#define MAX_REQUEST_SIZE 2047

struct client_info {  /*---------client 的 address structure-----------*/
    socklen_t address_length; // client_address_len
    struct sockaddr_storage address; // client_address （從sockaddr_storage拿的)
    SOCKET socket;    //client對應到的socket
    char request[MAX_REQUEST_SIZE + 1]; 
    int received;
    struct client_info *next; 
};

static struct client_info *clients = 0;

struct client_info *get_client(SOCKET s) { /*------找到指定的client-------*/
    struct client_info *ci = clients; //ci 代表客戶的linked_list

    while(ci) {
        if (ci->socket == s) //找到相對的客戶s
            break;
        ci = ci->next;
    }

    if (ci) return ci; //找到的話就回傳客戶s,沒找到就創建client_space
    struct client_info *n =
        (struct client_info*) calloc(1, sizeof(struct client_info));

    if (!n) { //沒有創建client_space成功的
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }

    n->address_length = sizeof(n->address);
    n->next = clients;
    clients = n;
    return n; //第n位客戶塞進queue裡面
}


void drop_client(struct client_info *client) { /*-----------把client清除掉-------------*/
    CLOSESOCKET(client->socket);

    struct client_info **p = &clients;

    while(*p) {
        if (*p == client) {
            *p = client->next;
            free(client);
            return;
        }
        p = &(*p)->next;
    }

    fprintf(stderr, "drop_client not found.\n");
    exit(1);
}


const char *get_client_address(struct client_info *ci) { /*-----取得該client的address info-------*/
    static char address_buffer[100];
    getnameinfo((struct sockaddr*)&ci->address,
            ci->address_length,
            address_buffer, sizeof(address_buffer), 0, 0,
            NI_NUMERICHOST);
    return address_buffer; //把該client的info 丟進 address_buffer裡面,再回傳給該client
}




fd_set wait_on_clients(SOCKET server) { //等待某個client 變成 waiting的狀態
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(server, &reads);
    SOCKET max_socket = server;

    struct client_info *ci = clients;

    while(ci) {
        FD_SET(ci->socket, &reads);
        if (ci->socket > max_socket)
            max_socket = ci->socket;
        ci = ci->next;
    }

    if (select(max_socket+1, &reads, 0, 0, 0) < 0) { //select 可以監控muti_socket,等到有socket的狀態變成"ready" for some class of I/O
        fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return reads;
}


void send_400(struct client_info *client) { //send error 給該client,並讓它斷線 -> 連線品質不佳
    const char *c400 = "HTTP/1.1 400 Bad Request\r\n"
        "Connection: close\r\n"
        "Content-Length: 11\r\n\r\nBad Request";
    send(client->socket, c400, strlen(c400), 0);
    drop_client(client);
}

void send_404(struct client_info *client) { //send error 給該client,並讓它斷線 -> 找不到該網頁
    const char *c404 = "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "Content-Length: 9\r\n\r\nNot Found";
    send(client->socket, c404, strlen(c404), 0);
    drop_client(client);
}



void serve_resource(struct client_info *client, const char *path) {

    printf("serve_resource %s %s\n", get_client_address(client), path);

    if (strcmp(path, "/") == 0) path = "/form.html";

    if (strlen(path) > 100) {
        send_400(client);
        return;
    }

    if (strstr(path, "..")) {
        send_404(client);
        return;
    }

    char full_path[128];
    sprintf(full_path, "public%s", path);

#if defined(_WIN32)
    char *p = full_path;
    while (*p) {
        if (*p == '/') *p = '\\';
        ++p;
    }
#endif

    FILE *fp = fopen(full_path, "rb");

    if (!fp) {
        send_404(client);
        return;
    }

    fseek(fp, 0L, SEEK_END);
    size_t cl = ftell(fp);
    rewind(fp);

    const char *ct = get_content_type(full_path);

#define BSIZE 1024
    char buffer[BSIZE];

    sprintf(buffer, "HTTP/1.1 200 OK\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Connection: close\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Length: %u\r\n", cl);
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Type: %s\r\n", ct);
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    int r = fread(buffer, 1, BSIZE, fp);
    while (r) {
        send(client->socket, buffer, r, 0);
        r = fread(buffer, 1, BSIZE, fp);
    }

    fclose(fp);
    drop_client(client);
}


int main() {
	char *lastqtr = NULL;
	int stfd = 0;
	stfd = open("test",O_RDWR | O_CREAT,S_IRWXU);

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif

    SOCKET server = create_socket(0, "8080");

    while(1) {

        fd_set reads;
        reads = wait_on_clients(server);

        if (FD_ISSET(server, &reads)) {
            struct client_info *client = get_client(-1);

            client->socket = accept(server,
                    (struct sockaddr*) &(client->address),
                    &(client->address_length));

            if (!ISVALIDSOCKET(client->socket)) {
                fprintf(stderr, "accept() failed. (%d)\n",
                        GETSOCKETERRNO());
                return 1;
            }


            printf("New connection from %s.\n",
                    get_client_address(client));
        }


        struct client_info *client = clients;
        while(client) {
            struct client_info *next = client->next;

            if (FD_ISSET(client->socket, &reads)) {

                if (MAX_REQUEST_SIZE == client->received) {
                    send_400(client);
                    client = next;
                    continue;
                }

                int r = recv(client->socket,
                        client->request + client->received,
                        MAX_REQUEST_SIZE - client->received, 0);

                if (r < 1) {
                    printf("Unexpected disconnect from %s.\n",
                            get_client_address(client));
                    drop_client(client);

                } else {
                    client->received += r;
                    client->request[client->received] = 0;

                    char *q = strstr(client->request, "\r\n\r\n");
                    if (q) {
                        //*q = 0;
			printf("This is !!! %s\n",q);	
                        if (strncmp("GET /", client->request, 5)) {
                            /*---------Post response------------------*/
				printf("client->request: %s\n",client->request);
				q = strstr(q,"Content-Type:");
				q = strstr(q,"\r\n\r\n");
				q = q + 4;
				printf("q = %s\n",q);
				lastqtr = strstr(q,"\r\n");
				*lastqtr = '\0';
				printf("Final!!! = %s",q);
				write(stfd,q,strlen(q));
                        } else {
                            char *path = client->request + 4; 
                            char *end_path = strstr(path, " ");
                            if (!end_path) {
                                send_400(client);
                            } else {
                                *end_path = 0;
                                serve_resource(client, path);
                            }
                        }
                    } //if (q)
                }
            }

            client = next;
        }

    } //while(1)


    printf("\nClosing socket...\n");
    CLOSESOCKET(server);


#if defined(_WIN32)
    WSACleanup();
#endif

    printf("Finished.\n");
    return 0;
}
