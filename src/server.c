/*!
 *  \file server.c
 *  \brief Implementacao das funcoes de servidor
 */

#include "server.h"

const int methods_num = 2;
const char *supported_methods[] = {"GET", "PUT"};
const int protocols_num = 2;
const char *supported_protocols[] = {"HTTP/1.0", "HTTP/1.1"};

/*! \brief Funcao verifica uma linha dupla em um buffer que e' uma string
 *
 * \param[in] buffer Uma string contendo a mensagem
 *
 * \return 1 Caso tenha encontrado o fim da mensagem
 * \return 0 Caso nao tenha encontrado o fim da mensagem
 */
static int verify_double_line(const char *buffer)
{
  if (strstr(buffer, "\r\n\r\n") == NULL)
    if (strstr(buffer, "\n\n") == NULL)
      return 0;
    
  return 1;
}

/*! \brief Verifica o metodo passado na requisicao do cliente
 *
 * \param[in] method_str O metodo extraido a partir da requisicao do cliente
 * \param[out] cli_method Variavel que armazena qual o metodo passado pelo
 * cliente
 *
 * \return READ_OK Caso ok
 * \return WRONG_READ Caso haja algum erro na leitura
 */
static int verify_cli_method(const char *method_str, 
                             Known_methods *cli_method)
{
  int cont = 0;
  int cmp_return = 0;

  do
  {
    cmp_return = strncmp(method_str, supported_methods[cont],
                         strlen(supported_methods[cont]));
    cont++;
  } while (cont < methods_num && cmp_return != 0);

  if (cmp_return != 0)
    return WRONG_READ;
  else
  {
    *cli_method = cont - 1;
    return READ_OK;
  }
}

/*! Funcao que analisa o root e o resource e monta um full path
 *
 * \param[in] resource O recurso extraido da mensagem do cliente
 * \param[out] full_path O caminho completo do recurso solicitado
 */
static void process_path(const char *resource, char *full_path)
{
  if (*resource == '/' && full_path[strlen(full_path) - 1] == '/')
    strcat(full_path, resource + 1);
  else if (*resource == '/' || full_path[strlen(full_path) - 1] == '/')
    strcat(full_path, resource);
  else
    strcat(full_path, strcat("/", resource));
}

/*! \brief Funcao que verifica o recurso que o cliente esta querendo acessar
 *
 * \param[in] resource String extraida da requisicao do cliente
 * correspondente ao recurso
 * \param[in] serv_root Path do root do servidor
 * \param[out] code Variavel que armazena o codigo http da resposta do cliente
 *
 * \return File correspondente ao recurso, caso OK
 * \return NULL caso algum erro
 */
static FILE *verify_cli_resource(const char *resource, 
                                 char *serv_root, Http_code *code)
{
  char full_path[RESOURCE_LEN];
  FILE *file = NULL;

  memset(full_path, 0, sizeof(full_path));
  strcpy(full_path, serv_root);

  if (strstr(resource, "../") != NULL)
  {
    *code = FORBIDDEN;
    return NULL;
  }

  process_path(resource, full_path);
  file = fopen(full_path, "r");
  if (!file)
  {
    *code = NOT_FOUND;
    return NULL;
  }

  return file;
}

/*! \brief Funcao que analisa um codigo http e retorna a string do status
 *
 * \param[in] http_code Variavel que armazena o codigo http para a resposta
 * ao cliente
 *
 * \return Char correspondente ao codigo
 */
static char *http_code_char(const Http_code http_code)
{
  switch (http_code)
  {
    case OK:
      return "OK";
      break;

    case BAD_REQUEST:
      return "BAD REQUEST";
      break;

    case FORBIDDEN:
      return "FORBIDDEN";
      break;

    case NOT_FOUND:
      return "NOT FOUND";
      break;

    case NOT_IMPLEMENTED:
      return "NOT IMPLEMENTED";
      break;
  }

  return NULL;
}

/*! \brief Gera o header da resposta ao cliente
 *
 * \param[in] cliente Estrutura que contem todas as informacoes sobre o cliente
 * e sobre a requisicao feita
 *
 * \return 0 Caso ok
 * \return -1 Caso haja algum erro
 */
static int create_header(Client *client)
{
  char header[HEADER_LEN];
  int resp_status = 0;
  int printf_return = 0;
  
  resp_status = client->resp_status;
  memset(header, 0, sizeof(header));

  printf_return = snprintf(header, HEADER_LEN,  "%s %d %s\r\n\r\n", 
         supported_protocols[client->protocol], resp_status,
         http_code_char(client->resp_status));
  if (printf_return >= HEADER_LEN)
    return -1;

  strcpy(client->buffer,header);
  client->pos_buf = strlen(client->buffer);
  return 0;
}

/*! \brief Verifica qual o protocolo da request do cliente
 *
 * \param[in] protocol O protocolo extraido da request do cliente
 * \param[out] cli_protocol Variavel que armazena qual o protocolo passado
 * pelo cliente
 *
 * \return READ_OK Caso ok
 * \return WRONG_READ Caso haja algum erro
 */
static int verify_cli_protocol(char *protocol, 
                               Known_protocols *cli_protocol)
{
  int cont = 0;
  int cmp_return = 0;

  do
  {
    cmp_return = strncmp(protocol, supported_protocols[cont],
                         strlen(supported_protocols[cont]));
    cont++;
  } while (cont < protocols_num && cmp_return != 0);

  if (cmp_return != 0)
    return WRONG_READ;
  else
  {
    *cli_protocol = cont - 1;
    return READ_OK;
  }
}

/*! \brief Funcao que analisa a requisicao do cliente e separa em tres strings,
 * o metodo, o recurso e o protocolo usados
 *
 * \param[out] client O cliente com todas as informacoes
 * \param[out] method O metodo da requisicao
 * \param[out] resource O recurso solicitado
 * \param[out] protocol O protocolo usado na requisicao
 *
 * \return READ_OK Caso ok
 * \return WRONG_READ Caso haja algum erro
 */
static int extr_req_params(Client *client, char *method, 
                           char *resource, char *protocol)
{
  int num_read = 0;

  memset(method, 0, sizeof(*method));
  memset(resource, 0, sizeof(*resource));
  memset(protocol, 0, sizeof(*protocol));

  num_read = sscanf(client->buffer, "%" STR(METHOD_LEN) "[^ ] %"
                    STR(RESOURCE_LEN) "[^ ] %" STR(PROTOCOL_LEN) 
                    "[^\r\n]", method, resource, protocol);
  
  memset(client->buffer, 0, sizeof(*client->buffer));
  client->pos_buf = 0;
  
  if (num_read != 3)
    return WRONG_READ;

  return READ_OK;
}

/*! \brief Verifica os argumentos passados para o servidor
 *
 * \param[in] argc Numero de argumentos
 * \param[in] argv Argumentos recebidos com informacoes de porta e root
 * \param[out] server Estrutura do servidor, para determinar a porta de escuta
 *
 * \return -1 caso algum erro tenha sido detectado
 * \return 0 caso OK
 */
int analyse_arguments(int argc, const char *argv[], Server *server)
{
  char *endptr = NULL;
  int arg_len = 0;

  if (argc < 3)
    return -1;

  if ((arg_len = strlen(argv[2])) >= PORT_LEN)
    return -1;
  server->listen_port = strtol(argv[2], &endptr, PORT_NUMBER_BASE);
  arg_len = strlen(argv[2]);
  if(endptr - argv[2] < arg_len)
    return -1;

  if ((arg_len = strlen(argv[1])) >= ROOT_LEN)
    return -1;
  strncpy(server->serv_root, argv[1], strlen(argv[1]));

  return 0;
}

/*! \brief Cria o socket para escuta em porta passada como parametro
 *
 * \param[in] server Estrutura do servidor, para usar a porta de escuta
 * \param[in] listen_backlog Numero de conexoes em espera
 *
 * \return -1 caso ocorra algum erro
 * \return socket caso esteja ok
 */
int create_listen_socket(const Server *server, int listen_backlog)
{
  struct sockaddr_in servaddr;
  int listen_socket;
  int yes = 1;

  listen_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_socket == 1)
    return -1;

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(server->listen_port);

  setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes,
             sizeof(int));
  
  if (bind(listen_socket, (struct sockaddr *)&servaddr,
    sizeof(servaddr)) == -1)
    return -1;

  if (listen(listen_socket, listen_backlog) == -1)
    return -1;

  return listen_socket;
}

/*! \brief Aceita novas conexoes e aloca no vetor de clientes
 *
 * \param[in] server A estrutura servidor para a conexao de um cliente
 *
 * \return -1 caso aconteça algum erro
 * \return 0 caso OK
 */
int make_connection(Server *server)
{
  int connfd = -1, i = 0;
  struct sockaddr_in cliaddr;
  socklen_t cli_length = sizeof(cliaddr);

  connfd = accept(server->listenfd, (struct sockaddr *)&cliaddr, 
                  &cli_length);
  if (connfd < 0)
    return -1;

  for (i = 0; i < FD_SETSIZE; i++)
    if (server->Client[i].sockfd < 0)
    {
      server->Client[i].sockfd = connfd;
      break;
    }

  if (i == FD_SETSIZE)
    return -1;

  FD_SET(connfd, &server->sets.read_s);
 
  server->maxfd_number = MAX(connfd, server->maxfd_number);
  server->max_cli_index = MAX(i, server->max_cli_index);

  return 0;
}

/*! \brief Fecha a conexao com um cliente 
 *
 * \param[out] client O cliente a ser considerado 
 * \param[out] set O fd_set em que esta o cliente
 */
void close_client_connection(Client *client, fd_set *set)
{
  FD_CLR(client->sockfd, set);
  close(client->sockfd); 

  if (client->buffer != NULL)
    free(client->buffer);

  if (client->file != NULL)
    fclose(client->file);
  
  memset(client, 0, sizeof(*client));
  client->sockfd = -1;
}

/*! \brief Inicializa a estrutura Server
 *
 * \param[out] server A estrutura a ser inicializada
 */
void init_server(Server *server)
{
  int i = 0;
  memset(server, 0, sizeof(*server));
  server->max_cli_index = -1;
  server->maxfd_number = -1;
  
  for (i = 0; i < FD_SETSIZE; i++)
    server->Client[i].sockfd = -1;
}

/*! \brief Inicializa os fd_sets e a referencia do maior descritor
 *
 * \param[out] server Estrutura do servidor
 */
void init_sets(Server *server)
{
  int i = 0;

  FD_ZERO(&server->sets.read_s);
  FD_ZERO(&server->sets.write_s);
  FD_ZERO(&server->sets.except_s);
  FD_SET(server->listenfd, &server->sets.read_s);
  server->maxfd_number = server->listenfd;

  for (i = 0; i <= server->max_cli_index; i++)
  {
    int cur_sock = server->Client[i].sockfd;
    if (cur_sock < 0)
      continue;

    server->maxfd_number = MAX(cur_sock, server->maxfd_number);

    if(server->Client[i].request_flag)
      FD_SET(cur_sock, &server->sets.write_s);
    else
      FD_SET(cur_sock, &server->sets.read_s);

    FD_SET(cur_sock, &server->sets.except_s);
  }
}

/*! \brief Funcao que testa se ha buffer alocado, aloca caso 
 * necessario, e recebe uma mensagem (ou parte dela)
 *
 * \param[out] server A estrutura do servidor
 *
 * \return enum Read_status_ que lista os possiveis casos de erros
 *
 * \notes E' feito um calloc do buffer_in de server_Client
 */
int read_client_input(Client *client)
{
  int n_bytes = 0;

  if (client->buffer == NULL)
  {
    client->buffer = (char *) calloc(BUFFER_LEN, sizeof(char));

    /* O que fazer quando nao puder alocar ? */
    if (client->buffer == NULL)
      return COULD_NOT_ALLOCATE;
  }
   
  if (client->pos_buf == BUFFER_LEN - 1)
    return BUFFER_OVERFLOW;

  if ((n_bytes = read(client->sockfd, client->buffer +
                      client->pos_buf, BUFFER_LEN - client->pos_buf - 1)) >  0)
  {
    client->pos_buf += n_bytes;
    return READ_OK;
  }

  if (n_bytes == 0)
    return ZERO_READ;
    
  return COULD_NOT_READ;
}

/*! \brief Verifica entrada de dados do client e determina quando a mensagem
 * chegou ao fim
 *
 * \param[out] client A estrutura de Client
 */
int verify_client_msg (Client *client)
{
  int read_return = 0;

  if ((read_return = read_client_input(client)) == READ_OK)
    client->request_flag = verify_double_line(client->buffer);

  return read_return;  
}

/* \brief Verifique a mensagem recebida de um cliente, analisando o metodo, o
 * procotolo e o recurso solicitado
 *
 * \param[out] client Contem informacoes a respeito do cliente
 * \param[in] serv_root Caminho do root do servidor
 *
 * \return READ_OK Caso leituras estejam ok
 * \return WRONG_READ Caso haja algum erro na leitura da request
 */
int verify_request(Client *client, char *serv_root)
{
  char method[METHOD_LEN + 1];
  char resource[RESOURCE_LEN + 1];
  char protocol[PROTOCOL_LEN + 1];
  Http_code code = 0;

  if (extr_req_params(client, method, resource, protocol) != READ_OK)
  {
    client->resp_status = BAD_REQUEST;
    return WRONG_READ;
  }

  if (verify_cli_method(method, &client->method) != READ_OK)
  {
    client->resp_status = NOT_IMPLEMENTED;
    return WRONG_READ;
  }

  if ((client->file = verify_cli_resource(resource, serv_root, &code))
       == NULL)
  {
    client->resp_status = code;
    return WRONG_READ;
  }

  if (verify_cli_protocol(protocol, &client->protocol) != READ_OK)
  {
    client->resp_status = BAD_REQUEST;
    return WRONG_READ;
  }

  client->resp_status = OK;
  return READ_OK;
}

/*! \brief Funcao que gera a resposta ao cliente e armazena em um buffer
 *
 * \param[out] client Estrutura que contem o buffer e informacoes do cliente
 *
 * \return 0 Caso OK
 * \return -1 Caso algum erro
 */
int build_response(Client *client)
{
  int bytes_read = 0;
 
  if (client->pos_buf == 0)
  {
    if (create_header(client) < 0)
      return -1;
  }
  else
  {
    memset(client->buffer, 0, sizeof(*client->buffer));
    client->pos_buf = 0;
  }
  
  if (client->resp_status == OK)
  {
    bytes_read = fread(client->buffer + client->pos_buf, sizeof(char),
                       BUFFER_LEN - client->pos_buf, client->file);
    if (bytes_read == 0 || ferror(client->file) != 0)
      return -1;
  }

  if (bytes_read < BUFFER_LEN - client->pos_buf)
    client->request_flag = 0;

  client->pos_buf += bytes_read;
  return 0;
}

/*! \brief Manda uma resposta para um cliente conectado atraves de um buffer
 * armazenado
 *
 * \param[in] client Variavel que armazena informacoes do cliente
 *
 * \return 0 Caso OK
 * \return -1 Caso haja erro
 */
 int send_response(Client *client)
{
  int cont_send = 0;
  int sent_bytes = 0;

  do
  {
    sent_bytes = send(client->sockfd, client->buffer, client->pos_buf, 0);
    cont_send++;

    if (sent_bytes == -1 && errno == EINTR)
      usleep(300);

  } while (sent_bytes == -1 && errno == EINTR && 
           cont_send <= LIMIT_SEND);

  if (cont_send == LIMIT_SEND)
    return -1;

  return 0;
}

