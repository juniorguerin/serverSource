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
#include <stddef.h>
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
#define REQUEST_SIZE 1024
#define LISTEN_BACKLOG 512
#define ROOT_LEN PATH_MAX
#define NUMBER_BASE 10
#define PORT_LEN 8
#define VEL_LEN 12
#define PROTOCOL_LEN 9
#define METHOD_LEN 5 
#define RESOURCE_LEN 200
#define STR_PROTOCOL_LEN STR(PROTOCOL_LEN)
#define STR_METHOD_LEN STR(METHOD_LEN)
#define STR_RESOURCE_LEN STR(RESOURCE_LEN)
#define LSOCK_NAME "/home/junior/Documentos/treinamento/treinamento.socket"
#define CONFIG_PATH "/home/junior/Documentos/treinamento/serverConfig/"
#define PID_FILE "servidorWeb.pid"
#define CONFIG_FILE "servidorWebConfig.txt"
#define LOG_FILE "log.txt"
#define PID_LEN 10
#define CONFIG_PARAM_NUM 3

#define READ_REQUEST 0x01
#define REQUEST_RECEIVED 0x02
#define WRITE_HEADER 0x04
#define WRITE_DATA 0x08
#define READ_DATA 0x10
#define SIGNAL_WAIT 0x20
#define PENDING_DATA 0x40
#define FINISHED 0x80

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

/*! \brief Arquivo sendo alterado / recebido */
typedef struct file_node_
{
  char file_name[RESOURCE_LEN]; /*!< Nome do arquivo */
  http_methods op_kind; /*!< Tipo de alteracao sendo realizada */
  int cont; /*!< Quantidade de mesmo tipo do arquivo */
  struct file_node_ *next;
  struct file_node_ *prev;
} file_node;

/* \brief Lista de arquivos sendo alterado / recebido */
typedef struct file_list_
{
  file_node *head; /* Primeiro arquivo */
  int size; /* Tamanho da lista*/
} file_list;

void file_node_append(file_node *file, file_list *l_of_files);
file_node *file_node_pop(file_node *f_node, file_list *l_files);

void file_node_free(file_node *file);
file_node *file_node_allocate(const char *file_name, http_methods method);

int verify_file_status(const char *file_name, http_methods cli_method,
                       file_list *l_files, file_node *match_file);

/*! \brief Um no' para a lista de clientes  */
typedef struct client_node_ 
{
  int sockfd; /*!< Socket de conexao */
  char *buffer; /*!< Buffer do cliente */
  int pos_buf; /*!< Posicao da escrita no buffer */
  int pos_header; /*!< Posicao do fim do header */
  int b_to_transfer; /*!< Bytes a transferir */
  task_status task_st; /*!< Status da tarefa do cliente */
  unsigned char status; /*!< Flags para o estado do cliente */
  http_methods method; /*!< Metodo usado na request */
  http_protocols protocol; /*!< Protocolo usado na request */
  http_code resp_status; /*!< Codigo para a resposta ao cliente */
  FILE *file; /*!< Arquivo para o recurso solicitado */
  token_bucket bucket; /*!< Bucket para controle de velocidade */
  file_node *used_file; /*!< Endereco do arquivo sendo usado */
  struct client_node_ *next; /*!< Proximo no' */
  struct client_node_ *prev; /*!< No' anterior */
} client_node;

/*! \brief Lista de clients  */
typedef struct client_list_
{
  client_node *head; /*!< Primeiro elemento da lista */
  int size; /*!< Tamanho da lista atual */
} client_list; 

void client_node_append(client_node *client, 
                        client_list *list_of_clients);
int client_node_pop(client_node *client, 
                    client_list *list_of_clients);

client_node *client_node_allocate(int sockfd);
void client_node_free(client_node *client);

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
  client_list l_clients; /*!< Lista de clientes conectados */
  server_fd_sets sets; /*!< Os fd_sets */
  long listen_port; /*!< A porta de escuta do servidor */
  int listenfd; /*!< O socket de escuta */
  int l_socket; /*!< Socket de escuta local */
  int maxfd_number; /*!< O maior descritor a observar */
  char serv_root[PATH_MAX]; /*!< O endereco do root do servidor */
  unsigned int velocity; /*!< Velocidade de conexao */
  struct timespec last_burst; /*!< Ultimo inicio de burst */
  threadpool thread_pool; /*!< Pool de threads */
  file_list used_files; /*! Arquivos que estao sendo escritos */
  client_node* cli_signaled[FD_SETSIZE]; /*!< Vetor de sinalizacao */
} server;

int server_init(int argc, const char **argv, server *r_server);

int server_select_analysis(server *r_server, struct timespec **timeout,
                           struct timespec *burst_rem_time);

int server_make_connection(server *r_server);

int server_client_remove(client_node **cur_client, server *r_server);

int server_recv_client_request(int bytes_to_receive,
                               client_node *cur_client);

int server_read_client_request(client_node *cur_client);
int server_verify_request(server *r_server, client_node *client);

int server_build_header(client_node *cur_client);

void server_read_file(void *cur_client);
void server_write_file(void *c_client);

int server_recv_response(client_node *client);
int server_send_response(client_node *cur_client);

void server_recv_thread_signals(server *r_server);
void server_process_thread_signals(server *r_server);

int server_process_read_file(client_node *client, server *r_server);
int server_process_write_file(client_node *client, server *r_server);

void server_process_cli_status(client_node *client);

void clean_up_server(server *r_server);

int server_write_pid_file();

void alter_config(server *r_server);

#endif
