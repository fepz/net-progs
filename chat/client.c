/*
 * Simple chat program using UDP and threads.
 * Write to stdin, read from stdout.
 * Author: fep.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_LINE 100

/**
 * Private functions.
 */
void* send_thread(void* args);
void* recv_thread(void* args);
int command_parser(const char* ucmd, char* cmd);
void cmd_register(const char* ucmd, char* cmd);

/**
 * Global declarations.
 */
struct sockaddr_in dest_addr;
socklen_t dest_addr_len = 0;
int sock;

int main(int argc, char *argv[])
{
    pthread_t recvt, sendt;

    struct sockaddr_in my_addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&my_addr, 0, sizeof(struct sockaddr_in));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = (in_port_t) 0;
    my_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(sock, (struct sockaddr*) &my_addr, (socklen_t) sizeof(struct sockaddr_in)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    socklen_t my_addr_len = (socklen_t) sizeof(my_addr);
    if (getsockname(sock, (struct sockaddr*) &my_addr, &my_addr_len) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }
    printf("Listening on: %s:%d\n", inet_ntoa(my_addr.sin_addr), ntohs(my_addr.sin_port));

    dest_addr_len = sizeof(dest_addr);

    if (argc == 3) { // IP:PORT
        memset(&dest_addr, 0, sizeof(struct sockaddr_in));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(atoi(argv[2]));
        inet_aton(argv[1], &(dest_addr.sin_addr));
        printf("Sending messages to: %s:%d\n", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
    }

    if (argc == 2) { // Only PORT
        memset(&dest_addr, 0, sizeof(struct sockaddr_in));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(atoi(argv[1]));
        dest_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        printf("Sending messages to: %s:%d\n", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
    }

    printf("^C to exit.\n");

    if (pthread_create(&recvt, NULL, recv_thread, NULL) > 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&sendt, NULL, send_thread, NULL) > 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_join(recvt, NULL) > 0) {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    if (pthread_join(sendt, NULL) > 0) {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

void* send_thread(void* args)
{
    char *buf = malloc(MAX_LINE);
    char *cmd = malloc(MAX_LINE);
    //int buflen = 0;
    do {
        printf("> "); fflush(stdout);
        fgets(buf, MAX_LINE, stdin);
        command_parser(buf, cmd);
        printf("%s\n", cmd);
        buf[strcspn(buf, "\n")] = '\0'; // removes new line
        //buflen = strlen(buf);
        //sendto(sock, &buflen, sizeof(buflen), 0, (struct sockaddr*) &dest_addr, sizeof(struct sockaddr_in));
        sendto(sock, cmd, strlen(cmd), 0, (struct sockaddr*) &dest_addr, sizeof(struct sockaddr_in));
    //} while (buflen > 0);
    } while (1);

    exit(EXIT_SUCCESS);
}

void* recv_thread(void* args)
{
    char *buf = malloc(MAX_LINE);
    do {
        fflush(stdout);
        //recvfrom(sock, &buflen, sizeof(buflen), 0, (struct sockaddr*) &dest_addr, &dest_addr_len);
        recvfrom(sock, buf, MAX_LINE, 0, (struct sockaddr*) &dest_addr, &dest_addr_len);
        //buf[r] = '\0';
        // Use VT100 escape code to delete the > character
        printf("\33[2K\r[%s:%d]: %s\n> ", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port), buf);
    } while(1);

    exit(EXIT_SUCCESS);
}

int command_parser(const char* ucmd, char* cmd)
{
    const char* pcmd = ucmd;
    
    if (*(pcmd++) != '/')
        return 0;

    switch (*pcmd) {
        case 'r':
            printf("Register\n");
            cmd_register(ucmd, cmd);
            break;
        case 'q':
            printf("Quit\n");
            exit(EXIT_SUCCESS);
        default:
            printf("Unknown command\n");
    }
    
    return 0;
}

void cmd_register(const char* ucmd, char* cmd)
{
    char name[25];
    sscanf(ucmd, "/r %s", name);
    sprintf(cmd, "R%s", name);
}

