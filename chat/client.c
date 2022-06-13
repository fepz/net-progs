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
int command_parser(const char* ucmd, char* cmd);
void cmd_login(const char* ucmd, char* cmd);
void cmd_register(const char* ucmd, char* cmd);
void cmd_sendmsg(const char* ucmd, char* cmd);

/**
 * Global declarations.
 */
struct sockaddr_in dest_addr;
socklen_t dest_addr_len = 0;
int sock;

static int logged = 0;
static char username[25];
static char prompt[30];

int main(int argc, char *argv[])
{
    pthread_t sendt;

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

    sprintf(prompt, "> ");

    if (pthread_create(&sendt, NULL, send_thread, NULL) > 0) {
        perror("pthread_create");
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
    do {
        printf("%s", prompt); fflush(stdout);
        fgets(buf, MAX_LINE, stdin);
        if (command_parser(buf, cmd) == 0) {
            printf("%s\n", cmd);
            buf[strcspn(buf, "\n")] = '\0'; // removes new line
            sendto(sock, cmd, strlen(cmd), 0, (struct sockaddr*) &dest_addr, 
                    sizeof(struct sockaddr_in));

            // Recibe respuesta del servidor.
            ssize_t n = recv(sock, buf, MAX_LINE, 0);
            if (n == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }
            printf("%s\n", buf);
        }
    } while (1);

    exit(EXIT_SUCCESS);
}

int command_parser(const char* ucmd, char* cmd)
{
    const char* pcmd = ucmd;
    
    if (*(pcmd++) != '/')
        return 1;

    switch (*pcmd) {
        case 'r':
            printf("Register\n");
            cmd_register(ucmd, cmd);
            return 0;
        case 'l':
            printf("Login\n");
            cmd_login(ucmd, cmd);
            return 0;
        case 's':
            printf("Send message\n");
            cmd_sendmsg(ucmd, cmd);
        case 'q':
            printf("Quit\n");
            exit(EXIT_SUCCESS);
        default:
            printf("Unknown command\n");
    }
    
    return 1;
}

void cmd_register(const char* ucmd, char* cmd)
{
    char name[25];
    sscanf(ucmd, "/r %s", name);
    sprintf(cmd, "R%s", name);
}

void cmd_login(const char* ucmd, char* cmd)
{
    char buf[25];
    logged = 1;
    sscanf(ucmd, "/l %s", buf);
    sprintf(cmd, "L%s", buf);
    bzero(username, sizeof(username));
    strncpy(username, buf, strlen(buf));
    bzero(prompt, sizeof(prompt));
    sprintf(prompt, "(%s) > ", username);
}

void cmd_sendmsg(const char* ucmd, char* cmd)
{
    char dst[25];
    char msg[100];
    sscanf(ucmd, "/s %s %s", dst, msg);
    printf("%s\n", dst);
    printf("%s\n", msg);
}

