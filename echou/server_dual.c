/**
 * Servidor echo UDP y TCP
 * -----------------------
 *
 * El servidor escucha en el IP:PUERTO indicado como parámetro en la línea de
 * comando, o 127.0.0.1:8888 en caso de que no se indique una dirección, tanto
 * paquetes UDP como conexiones TCP.
 * 
 * Al recibir un datagrama UDP imprime el contenido del mismo, precedido de la 
 * dirección del emisor.
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
#include <sys/select.h>
#include <signal.h>

// Dirección por defecto del servidor.
#define PORT 8888
#define IP   "127.0.0.1"

// Tamaño del buffer en donde se reciben los mensajes.
#define BUFSIZE 100

// Máximo número de conexiones pendientes en el socket TCP.
#define PENDING 10

// Cierra el socket al recibir una señal SIGTERM.
void handler(int signal)
{
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[])
{
    struct sockaddr_in addr;

    // Descriptores de archivo de los sockets.
    int socket_udp;
    int socket_tcp;

    // Configura el manejador de señal SIGTERM.
    signal(SIGTERM, handler);

    // Crea los sockets.
    socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_udp == -1) {
        perror("socket_udp");
        exit(EXIT_FAILURE);
    }
    socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_tcp == -1) {
        perror("socket_tcp");
        exit(EXIT_FAILURE);
    }

    // Dirección donde escuchará el servidor con TCP y UDP.
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
    if(setsockopt(socket_udp, SOL_SOCKET, optname, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    if(setsockopt(socket_tcp, SOL_SOCKET, optname, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Asocia los sockets con la dirección indicada. Tradicionalmente esta 
    // operación se conoce como "asignar un nombre al socket".
    int b;
    b = bind(socket_udp, (struct sockaddr*) &addr, sizeof(addr));
    if (b == -1) {
        perror("bind udp");
        exit(EXIT_FAILURE);
    }
    b = bind(socket_tcp, (struct sockaddr*) &addr, sizeof(addr));
    if (b == -1) {
        perror("bind tcp");
        exit(EXIT_FAILURE);
    }

    // Convierte el socket TCP en pasivo.
    listen(socket_tcp, PENDING);

    printf("Escuchando en %s:%d ...\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    
    char buf[BUFSIZE];
    struct sockaddr_in src_addr;
    socklen_t src_addr_len;
    fd_set read_fds;

    FD_ZERO(&read_fds);
    
    for(;;) {
        // Agrega los sockets UDP y TCP en el set de descriptores de archivos
        // que se monitorearan por medio de select().
        FD_SET(socket_udp, &read_fds);
        FD_SET(socket_tcp, &read_fds);

        // Espera hasta que exista un paquete o una conexión en los sockets.
        int s = select(socket_tcp + 1, &read_fds, NULL, NULL, NULL);
        if (s == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        memset(&src_addr, 0, sizeof(struct sockaddr_in));
        src_addr_len = sizeof(struct sockaddr_in);
        int sock;

        // Existe una conexión entrante en el socket TCP.
        if (FD_ISSET(socket_tcp, &read_fds)) {
            int n;
            sock = accept(socket_tcp, (struct sockaddr*) &src_addr, &src_addr_len);
            if (sock == -1) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            pid_t pid = fork();
            switch (pid) {
                case -1:
                    perror("fork");
                    exit(EXIT_FAILURE);
                case 0:
                    while ((n = read(sock, buf, BUFSIZE)) > 0) {
                        if (n == -1) {
                            perror("read");
                            exit(EXIT_FAILURE);
                        }
                        // Elimina '\n' al final del buffer.
                        buf[n-1] = '\0';
                        printf("[%s:%d][TCP] %s\n", inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), buf);

                        // Envía eco
                        buf[n-1] = '\n';
                        write(sock, buf, strlen(buf));
                    }
                    close(sock);
                    exit(EXIT_SUCCESS);
                default:
                    break;
            }
        }

        // Existe un paquete entrante en el socket UDP.
        if (FD_ISSET(socket_udp, &read_fds)) {

            // Recibe un mensaje entrante.
            ssize_t n = recvfrom(socket_udp, buf, BUFSIZE, 0, (struct sockaddr*) &src_addr, &src_addr_len); 
            if(n == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }

            // Elimina '\n' al final del buffer.
            buf[n-1] = '\0';

            // Imprime dirección del emisor y mensaje recibido.
            printf("[%s:%d][UDP] %s\n", inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), buf);

            // Envía eco
            buf[n-1] = '\n';
            n = sendto(socket_udp, buf, strlen(buf), 0, (struct sockaddr*) &src_addr, src_addr_len);
            if (n == -1) {
                perror("sendto");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Cierra los sockets.
    close(socket_udp);
    close(socket_tcp);

    exit(EXIT_SUCCESS);
}

