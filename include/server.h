/*!
 *  \file server.h
 *  \brief Header de funcoes utilizadas no servidor
 *
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
#include <sys/wait.h>
#include <sys/un.h>
#include <unistd.h>

#define BUFFER_LEN BUFSIZ
#define REQUEST_SIZE BUFSIZ
#define LISTEN_BACKLOG 512
#define ROOT_LEN 2048
#define PORT_NUMBER_BASE 10
#define PORT_LEN 8
#define PROTOCOL_LEN 9
#define METHOD_LEN 5 
#define RESOURCE_LEN 200
#define LIMIT_SEND 5
#define HEADER_LEN 100

#undef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#undef STR
#define STR_(x) #x
#define STR(x) STR_(x)

/* Informacoes sobre os metodos suportados */
extern const int methods_num;
extern const char *supported_methods[];
typedef enum Known_methods_
{
  GET,
  PUT
} Known_methods;

/* Informacoes sobre os protocolos suportados */
extern const char *supported_protocols[];
extern const int protocols_num;
typedef enum Known_protocols_
{
  HTTP10,
  HTTP11
} Known_protocols;

/* Os codigos http */
typedef enum Http_code_
{
  OK = 200,
  BAD_REQUEST = 400,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  NOT_IMPLEMENTED = 501
} Http_code;

/* Guarda informacoes sobre os clientes conectados */
typedef struct Client_ 
{
  int sockfd; /*!< Socket de conexao */
  char *buffer; /*!< Buffer do cliente */
  int pos_buf; /*!< Posicao da escrita no buffer */
  int request_flag; /*!< Status da comunicacao com o servidor */
  int nonblock_write; /*!< Status de leitura non-blocking */
  Known_methods method; /*!< Metodo usado na request */
  Known_protocols protocol; /*!< Protocolo usado na request */
  Http_code resp_status; /*!< Codigo para a resposta ao cliente */
  FILE *file; /*!< Arquivo para o recurso solicitado */

} Client;

/* Guarda as variaveis do tipo fd_set vinculadas ao servidor */
typedef struct Server_fd_sets_ 
{
  fd_set write_s; /*!< fd_set de escrita */
  fd_set read_s; /*!< fd_set de leitura */
  fd_set except_s; /*!< fd_set de excecao */
} Server_fd_sets;

/* Informacoes a respeito do estado atual do servidor */
typedef struct Server_ 
{
  Client Client[FD_SETSIZE]; /*!< Estrutura de clientes conectados */
  int max_cli_index; /*!< Maior indice utilizado na estrutura de clientes */
  Server_fd_sets sets; /*!< Os fd_sets */
  long listen_port; /*!< A porta de escuta do servidor */
  int listenfd; /*!< O socket de escuta */
  int maxfd_number; /*!< O maior descritor a observar */
  char serv_root[ROOT_LEN]; /*!< O endereco do root do servidor */
} Server;

int analyse_arguments(int argc, const char *argv[], Server *server);

int create_listen_socket(const Server *server, int listen_backlog);

int make_connection(Server *server);

void close_client_connection(Client *Client);

void init_server(Server *server);

void init_sets(Server *server);

int read_client_input(Client *client);

int recv_client_msg (Client *client);

int verify_request(Client *client, char *serv_root);

int build_response(Client *client);

int send_response(Client *client);

#endif
