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
#include <multithread.h>
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
#include <time.h>
#include <token_bucket.h>
#include <unistd.h>

#undef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#undef STR
#define STR_(x) #x
#define STR(x) STR_(x)

#define BUFFER_LEN BUFSIZ
#define REQUEST_SIZE BUFSIZ
#define LISTEN_BACKLOG 512
#define ROOT_LEN 2048
#define NUMBER_BASE 10
#define PORT_LEN 8
#define VEL_LEN 6
#define PROTOCOL_LEN 9
#define METHOD_LEN 5 
#define RESOURCE_LEN 200
#define STR_PROTOCOL_LEN STR(PROTOCOL_LEN)
#define STR_METHOD_LEN STR(METHOD_LEN)
#define STR_RESOURCE_LEN STR(RESOURCE_LEN)
#define LSOCKET_NAME 64
#define SIGNAL_MAX 128
#define SIGNAL_COL 2

#define PENDING_DATA 0x01
#define REQUEST_RECEIVED 0x02
#define WRITE_DATA 0x04
#define WRITE_HEADER 0x08
#define SIGNAL_OK 0x20
// 0x10 0x40 0x80

extern const char *supported_methods[];
typedef enum http_methods_
{
  GET,
  PUT,
  NUM_METHOD
} http_methods;

extern const char *supported_protocols[];
typedef enum http_protocols_
{
  HTTP10,
  HTTP11,
  NUM_PROTOCOL
} http_protocols;

typedef enum http_code_
{
  OK = 200,
  BAD_REQUEST = 400,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  NOT_IMPLEMENTED = 501
} http_code;

/*! \brief Um no' para a lista de clientes  */
typedef struct client_node_ 
{
  int sockfd; /*!< Socket de conexao */
  char *buffer; /*!< Buffer do cliente */
  int pos_buf; /*!< Posicao da escrita no buffer */
  int sent_buf; /*!< Posicao de envio no buffer */
  unsigned char status; /*!< Flags para o estado do cliente */
  http_methods method; /*!< Metodo usado na request */
  http_protocols protocol; /*!< Protocolo usado na request */
  http_code resp_status; /*!< Codigo para a resposta ao cliente */
  FILE *file; /*!< Arquivo para o recurso solicitado */
  token_bucket bucket; /*!< Bucket para controle de velocidade */
  struct client_node_ *next; /*!< Proximo no' */
  struct client_node_ *before; /*!< No' anterior */
} client_node;

/*! \brief Lista de clients  */
typedef struct client_list_
{
  client_node *head; /*!< Primeiro elemento da lista */
  int size; /*!< Tamanho da lista atual */
} client_list; 

void append_client(client_node *client, client_list *list_of_clients);

int pop_client(client_node *client, client_list *list_of_clients);

client_node *allocate_client_node(int sockfd);

void free_client_node(client_node *client);

int remove_client(client_node **client, 
                  client_list *list_of_clients); 

/*! \brief Guarda as variaveis do tipo fd_set vinculadas ao servidor
 */
typedef struct server_fd_sets_ 
{
  fd_set write_s; /*!< fd_set de escrita */
  fd_set read_s; /*!< fd_set de leitura */
  fd_set except_s; /*!< fd_set de excecao */
} server_fd_sets;

/*! \brief Informacoes a respeito do estado atual do servidor */
typedef struct server_ 
{
  client_list list_of_clients; /*!< Lista de clientes conectados */
  server_fd_sets sets; /*!< Os fd_sets */
  long listen_port; /*!< A porta de escuta do servidor */
  int listenfd; /*!< O socket de escuta */
  int l_socket; /*!< Socket de escuta local */
  char lsocket_name[LSOCKET_NAME]; /*!< Nome do socket local */
  int maxfd_number; /*!< O maior descritor a observar */
  char serv_root[ROOT_LEN]; /*!< O endereco do root do servidor */
  unsigned int velocity; /*!< Velocidade de conexao */
  struct timeval last_burst; /*!< Ultimo inicio de burst */
  threadpool thread_pool; /*!< Pool de threads */
  long signals[SIGNAL_MAX][SIGNAL_COL]; /*!< Vetor de sinalizacao */
} server;

int parse_arguments(int argc, const char *argv[], server *r_server);

int create_listenfd(const server *r_server);

int create_local_socket(const server *r_server);

int make_connection(server *r_server);

int init_server(server *r_server);

int init_sets(server *r_server);

int read_client_input(int bytes_to_receive, client_node *cur_client);

int recv_client_msg(client_node *cur_client);

void verify_request(char *serv_root, client_node *cur_client);

int send_header(client_node *cur_client);

void *read_file(void *cur_client);

int send_response(client_node *cur_client);

struct timeval burst_init(struct timeval *last_fill, 
                         const client_list *list_of_clients);

void recv_thread_signals(server *r_server);

void process_thread_signals(server *r_server);

#endif
