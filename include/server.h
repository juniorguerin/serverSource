/*!
 *  \file server.h
 *  \brief Header de funcoes utilizadas no servidor
 *
 *  "Id = 2"
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/un.h>

#define BUFFER_LEN BUFSIZ
#define REQUEST_SIZE BUFSIZ
#define LISTEN_BACKLOG 512
#define ROOT_LEN 2048
#define PORT_NUMBER_BASE 10
#define PORT_LEN 8

#undef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))

typedef enum Read_status_
{
  COULD_NOT_READ = -1,
  NO_END_READ = -2,
  COULD_NOT_ALLOCATE = -3,
  ZERO_READ = -4,
  BUFER_OVERFLOW = -5,
  READ_OK = 1
} Read_status;

typedef struct Clients_ {
  int sockfd;
  char *buffer;
  int bytes_buf;
  int write_flag;
} Clients;

typedef struct Server_fd_sets_ {
  fd_set write_s;
  fd_set read_s;
  fd_set except_s;
} Server_fd_sets;

typedef struct Server_ {
  Clients clients[FD_SETSIZE];
  int max_cli_index;
  Server_fd_sets sets;
  long listen_port;
  int listenfd;
  int maxfd_number;
  char serv_root[ROOT_LEN];
} Server;

int analise_arguments(int argc, const char *argv[], Server *server);

int create_listen_socket(const Server *server, int listen_backlog);

int make_connection(Server *server);

void close_client_connection(Clients *clients, fd_set *set);

void init_server(Server *server);

void init_sets(Server *server);

int read_client_input(Clients *client);

int verify_double_line(char *buffer);

int verify_client_msg (Clients *client);
