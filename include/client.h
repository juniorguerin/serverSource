/*!
 * \file client.h
 * \brief Header que contem definicoes de funcoes comumente usadas pelos
 * clientes http
 *
 * "Id: 1"
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <limits.h>

#define HOST_LEN 128
#define PORT_LEN 8
#define PATH_LEN 512
#define BUFFER_LEN BUFSIZ 
#define MESSAGE_LEN 512
#define FLAG_WRITE "-f"
#define HEADER_LEN BUFSIZ
#define HTTP_PORT "80"
#define LIMIT_SEND 10
#define URL_LEN 1024
#define HTTP_COD_LEN 256

/*! \brief Estrutura com as informacoes extraidas da url */
struct parsed_url
{
  char host[HOST_LEN];   /*!< Host da URL */
  char path[PATH_LEN];   /*!< Path da URL */
  char file_name[PATH_MAX];   /*!< Nome do arquivo */
  char port[PORT_LEN];   /*!< Numero da porta para conexao */
};

int get_url_info(const char *url, struct parsed_url *url_info, 
    const char *argv[], int file_name_pos);

void verify_double_line(char character, int *flag_n1, 
    int *flag_n2);

int  arguments_test(const char *argv[], int argc, int *overwrite_flag,
    int *file_name_pos);

int make_connection(int *sockfd, const char *host, const char *port);

FILE *file_create(const char *file_name, int overwrite_flag);

int send_message(const char *path, int sockfd);

int get_http_cod(const char *header, char *http_cod);

int recv_header(char *header, int sockfd);

int ident_begin_file(char *header);

#endif
