/*!
 *  \file server.h
 *  \brief Header de funcoes utilizadas no servidor
 *
 *  "Id = 2"
 */

#ifndef SERVER_H
#define SERVER_H

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
#define PROTOCOL_LEN 9
#define METHOD_LEN 5 
#define RESOURCE_LEN 200

#undef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#undef STR
#define STR_(x) #x
#define STR(x) STR_(x)

extern const int methods_num;
extern const char *supported_methods[];
typedef enum Known_methods_
{
  GET,
  PUT
} Known_methods;

extern const char supported_protocols[];

typedef enum Read_status_
{
  COULD_NOT_READ = -1,
  NO_END_READ = -2,
  COULD_NOT_ALLOCATE = -3,
  ZERO_READ = -4,
  BUFFER_OVERFLOW = -5,
  WRONG_READ = -6,
  READ_OK = 1
} Read_status;

typedef enum Http_code_
{
  OK = 200,
  BAD_REQUEST = 400,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  INTERNAL_ERROR = 500,
  NOT_IMPLEMENTED = 501
} Http_code;

typedef struct Client_ {
  int sockfd;
  char *buffer;
  int bytes_buf;
  int request_flag;
  Known_methods method;
  FILE *file;
  Http_code resp_status;
} Client;

typedef struct Server_fd_sets_ {
  fd_set write_s;
  fd_set read_s;
  fd_set except_s;
} Server_fd_sets;

typedef struct Server_ {
  Client Client[FD_SETSIZE];
  int max_cli_index;
  Server_fd_sets sets;
  long listen_port;
  int listenfd;
  int maxfd_number;
  char serv_root[ROOT_LEN];
} Server;

int analyse_arguments(int argc, const char *argv[], Server *server);

int create_listen_socket(const Server *server, int listen_backlog);

int make_connection(Server *server);

void close_client_connection(Client *Client, fd_set *set);

void init_server(Server *server);

void init_sets(Server *server);

int read_client_input(Client *client);

//int verify_double_line(const char *buffer);

int verify_client_msg (Client *client);

int verify_request(Client *client, char *serv_root);

//int verify_cli_method(char *method_str, Known_methods *cli_method);

//FILE *verify_cli_resource(char *resource, char *serv_root, Http_code *code);

int build_response(Client *client);

#endif
