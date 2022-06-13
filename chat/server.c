/**
 * Servidor Chat UDP
 * -----------------
 *
 * El servidor escucha en el IP:PUERTO indicado como parámetro en la línea de
 * comando, o 127.0.0.1:8888 en caso de que no se indique una dirección.
 *
 * Para terminar la ejecución del servidor, envíar una señal SIGTERM (^C)
 *
 * Se puede probar el funcionamiento del servidor con el programa netcat:
 *
 * nc -u 127.0.0.1 8888
 *
 *
 * ---
 * Autor: Francisco Paez
 * Fecha: 2022-06-03
 * Última modificacion: 2022-06-03
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>

// Dirección por defecto del servidor.
#define PORT 8888
#define IP   "127.0.0.1"

// Tamaño del buffer en donde se reciben los mensajes.
#define BUFSIZE 100

// Estructura de datos que almacena información de un usuario.
struct user {
    int id;                     // Identificador númerico único
    char name[50];              // Nombre del usuario
    int status;                 // Online - Offline
    struct sockaddr_in addr;    // Client addr
};
typedef struct user user_t;

// "Lista" de usuarios registrados
user_t users[100];

// Cierra el socket al recibir una señal SIGTERM.
void handler(int signal)
{
    exit(EXIT_SUCCESS);
}

int user_count(int op)
{
    int i, c = 0;
    switch(op) {
        case 0: // Cuenta todos los usuarios registrados
            for (i = 0; i < 100; i++) {
                if (users[i].id > 0) {
                    c = c + 1;
                }
            }
            return c;
        case 1: // Cuenta los usuario actualmente conectados
            for (i = 0; i < 100; i++) {
                if (users[i].id > 0 && users[i].status == 1) {
                    c = c + 1;
                }
            }
            return c;
        default:
            return -1;
    }
}

int user_registration(char* username)
{
    // Busca el primer lugar libre
    int i;
    for (i = 0; i < 100; i++) {
        if (users[i].id == 0) {
            users[i].id = i+1;
            users[i].status = 0;
            strncpy(users[i].name, username, strlen(username));
            return users[i].id;
        }
    }
    return -1;
}

int user_login(const char* username, const struct sockaddr_in addr)
{
    int i;
    for (i = 0; i < 100; i++) {
        if (strncmp(username, users[i].name, strlen(username)) == 0) {
            users[i].status = 1;
            users[i].addr = addr;
            printf("[%s:%d]\n", inet_ntoa(users[i].addr.sin_addr), ntohs(users[i].addr.sin_port));
            return users[i].status;
        }
    }
    return -1;
}

user_t* user_find(const char* username)
{
    int i;
    for (i = 0; i < 100; i++) {
        if (strncmp(username, users[i].name, strlen(username)) == 0) {
            return &(users[i]);
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    // Descriptor de archivo del socket.
    static int fd;

    // Dirección asociada al socket.
    struct sockaddr_in addr;

    // Configura el manejador de señal SIGTERM.
    signal(SIGTERM, handler);

    // Crea el socket.
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Estructura con la dirección donde escuchará el servidor.
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    if (argc == 3) {
        addr.sin_port = htons((uint16_t) atoi(argv[2]));
        inet_aton(argv[1], &(addr.sin_addr));
    } else {
        addr.sin_port = htons(PORT);
        inet_aton(IP, &(addr.sin_addr));
    }

    // Permite reutilizar la dirección que se asociará al socket.
    int optval = 1;
    int optname = SO_REUSEADDR | SO_REUSEPORT;
    if(setsockopt(fd, SOL_SOCKET, optname, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Asocia el socket con la dirección indicada. Tradicionalmente esta 
    // operación se conoce como "asignar un nombre al socket".
    int b = bind(fd, (struct sockaddr*) &addr, sizeof(addr));
    if (b == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Escuchando en %s:%d ...\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    
    char buf[BUFSIZE];
    char send_buf[BUFSIZE];
    struct sockaddr_in src_addr;
    socklen_t src_addr_len;
    
    for(;;) {
        memset(&src_addr, 0, sizeof(struct sockaddr_in));
        src_addr_len = sizeof(struct sockaddr_in);
        bzero(buf, BUFSIZE);
        bzero(send_buf, BUFSIZE);

        // Recibe mensaje de un cliente.
        ssize_t n = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr*) &src_addr, &src_addr_len); 
        if(n == -1) {
            perror("recv");
            exit(EXIT_FAILURE);
        }

        user_t *dest;
        char command = buf[0];
        printf("%s\n", buf);

        // Ejecuta el comando enviado por el cliente.
        switch(command) {
            case 'R':
                sprintf(send_buf, "%d", user_registration(&(buf[1])));
                break;
            case 'L':
                sprintf(send_buf, "%d", user_login(&(buf[1]), src_addr));
                break;
            case 'Q':
                sprintf(send_buf, "%d", user_count(atoi(&buf[1])));
                break;
            case 'S':
                dest = user_find(&buf[1]);
                sprintf(send_buf, "%s\n", &buf[2]);
                n = sendto(fd, send_buf, strlen(&buf[2])+1, 0, 
                        (struct sockaddr*) &(dest->addr), src_addr_len);
                sprintf(buf, "%ld\n", n);
                break;
            default:
                sprintf(buf, "E");
        }

        // Envía la respuesta al cliente.
        n = sendto(fd, send_buf, strlen(send_buf) + 1, 0, 
                (struct sockaddr*) &src_addr, src_addr_len);
        if (n == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
    }

    // Cierra el socket.
    close(fd);

    exit(EXIT_SUCCESS);
}

