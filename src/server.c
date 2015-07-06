/*!
 *  \file server.c
 *  \brief Implementacao das funcoes de servidor
 */

#include "server.h"

const char *supported_methods[] = {"GET", "PUT"};
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
  if (strstr(buffer, "\r\n\r\n") == NULL || strstr(buffer, "\n\n") == NULL)
    return 1;
    
  return 0;
}

/*! \brief Verifica o metodo passado na requisicao do Cliente
 *
 * \param[in] method_str O metodo extraido a partir da requisicao do cliente
 * \param[out] cur_client O cliente em questao 
 */
static void verify_cli_method(const char *method_str, 
                             client_node *cur_client)
{
  int cont = 0;
  int cmp_return = 0;

  do
  {
    cmp_return = strncmp(method_str, supported_methods[cont],
                         strlen(supported_methods[cont]));
    cont++;
  } while (cont < NUM_METHOD && cmp_return != 0);

  if (cmp_return != 0)
    cur_client->resp_status = NOT_IMPLEMENTED;
  else
    cur_client->method = cont - 1;
}

/*! \brief Funcao que verifica o recurso que o cliente esta querendo acessar
 *
 * \param[in] resource String extraida da requisicao do cliente
 * correspondente ao recurso
 * \param[in] serv_root Path do root do servidor
 * \param[out] cur_client O cliente em questao
 *
 * \return 0 Caso a analise chegue ao fim
 * \return -1 Caso se encaixe em algum dos casos de erro
 */
static int  verify_cli_resource(const char *resource, char *serv_root, 
                                client_node *cur_client)
{
  char full_path[PATH_MAX];
  char rel_path[PATH_MAX];

  memset(full_path, 0, sizeof(full_path));
  memset(rel_path, 0, sizeof(rel_path));

  if (strstr(resource, "../") != NULL)
  {
    cur_client->resp_status = FORBIDDEN;
    return -1;
  }

  strncpy(rel_path, serv_root, ROOT_LEN - 1);
  strncat(rel_path, resource, PATH_MAX - ROOT_LEN - 1);
  if (!realpath(rel_path, full_path))
  {
    cur_client->resp_status = NOT_FOUND;
    return -1;
  }

  cur_client->file = fopen(full_path, "r");
  if (!cur_client->file)
  {
    cur_client->resp_status = NOT_FOUND;
    return -1;
  }

  cur_client->resp_status = OK;
  return 0;
}

/*! \brief Funcao que analisa um codigo http e retorna a string do status
 *
 * \param[in] http_code Variavel que armazena o codigo http para a resposta
 * ao cliente
 *
 * \return Char correspondente ao codigo
 */
static char *http_code_char(const http_code http_code)
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
static int create_header(client_node *cur_client)
{
  int resp_status = 0;
  int printf_return = 0;
  
  resp_status = cur_client->resp_status;

  printf_return = snprintf(cur_client->buffer, BUFFER_LEN - 1,
                           "%s %d %s\r\n\r\n", 
                           supported_protocols[cur_client->protocol], 
                           resp_status, 
                           http_code_char(cur_client->resp_status));
  if (printf_return >= BUFFER_LEN || printf_return < 0)
    return -1;

  cur_client->pos_buf = printf_return;
  return 0;
}

/*! \brief Verifica qual o protocolo da request do cliente
 *
 * \param[in] protocol O protocolo extraido da request do cliente
 * \param[out] cur_client O cliente em questao
 */
static void verify_cli_protocol(const char *protocol, 
                               client_node *cur_client)
{
  int cont = 0;
  int cmp_return = 0;

  do
  {
    cmp_return = strncmp(protocol, supported_protocols[cont],
                         strlen(supported_protocols[cont]));
    cont++;
  } while (cont < NUM_PROTOCOL && cmp_return != 0);

  if (cmp_return != 0)
    cur_client->resp_status = BAD_REQUEST;
  else
    cur_client->protocol = cont - 1;
}

/*! \brief Funcao que analisa a requisicao do cliente e separa em tres 
 * strings, o metodo, o recurso e o protocolo usados
 *
 * \param[out] cur_client O cliente com todas as informacoes
 * \param[out] method O metodo da requisicao
 * \param[out] resource O recurso solicitado
 * \param[out] protocol O protocolo usado na requisicao
 */
static void extr_req_params(client_node *cur_client, char *method, 
                           char *resource, char *protocol)
{
  int num_read = 0;

  memset(method, 0, sizeof(*method));
  memset(resource, 0, sizeof(*resource));
  memset(protocol, 0, sizeof(*protocol));

  num_read = sscanf(cur_client->buffer, "%" STR_METHOD_LEN "[^ ] "
                    "%" STR_RESOURCE_LEN "[^ ] "
                    "%" STR_PROTOCOL_LEN "[^\r\n]", 
                    method, resource, protocol); 
  
  if (num_read != 3)
    cur_client->resp_status = BAD_REQUEST;
}

/*! \brief Aloca um novo elemento da estrutura de clientes
 * 
 * \param[in] sockfd O socket para o cliente
 *
 * \return NULL caso haja erro de alocacao
 * \return client caso o cliente seja alocado
 */
static client_node *allocate_new_client(int sockfd)
{
  client_node *new_client = NULL;

  new_client = (client_node *) calloc(1, sizeof(client_node));
  if (!new_client)
    return NULL;

  new_client->sockfd = sockfd;
  return new_client;
}

/*! \brief Libera um elemento da struct de cliente
 *
 * \param[out] client_remove o cliente a ser removido
 */
static void free_client_node (client_node *client_remove)
{
  close(client_remove->sockfd);
  if (client_remove->buffer)
    free(client_remove->buffer);
  if (client_remove->file)
    fclose(client_remove->file);
  free(client_remove);
}

/*! \brief Adiciona um cliente a lista de clientes
 *
 * \param[in] sockfd O socket do novo cliente
 * \param[out] cli_list O primeiro elemento da lista
 */
int add_client_to_list(const int sockfd, client_list *list_of_clients)
{
  client_node *last_client = NULL;
  client_node *new_client = NULL;

  new_client = allocate_new_client(sockfd);
  if (!new_client)
    return -1;

  if (!list_of_clients->head)
  {
    list_of_clients->head = new_client;
    list_of_clients->size++;
    return 0;
  }

  for (last_client = list_of_clients->head; last_client->next; 
       last_client = last_client->next)
    ;
  last_client->next = new_client;
  list_of_clients->size++;

  return 0;
}

/*! \brief Elimina a referencia de um elemento dentro da lsita
 *
 * \param[in] sockfd O socket do cliente a ser removido
 * \param[out] list_of_clients A lista de clientes
 *
 * \return -1 Caso nao consiga encontrar o elemento
 * \return 0 Caso ok
 */
int remove_client_from_list(const int sockfd, 
                            client_list *list_of_clients)
{
  client_node *client_before = NULL;
  client_node *client_remove = NULL;

  if (0 >= list_of_clients->size)
    return -1;

  for(client_remove = list_of_clients->head; 
      sockfd != client_remove->sockfd && client_remove->next;  
      client_before = client_remove, client_remove = client_remove->next)
    ;

  if (!client_remove)
    return -1;

  if (client_remove == list_of_clients->head)
    list_of_clients->head = client_remove->next;
  else
    client_before->next = client_remove->next;
  
  free_client_node(client_remove);
  list_of_clients->size--;

  return 0;
}

/*! \brief Verifica os argumentos passados para o servidor
 *
 * \param[in] argc Numero de argumentos
 * \param[in] argv Argumentos recebidos com informacoes de porta e root
 * \param[out] r_server Estrutura do servidor, para determinar a porta de escuta
 *
 * \return -1 caso algum erro tenha sido detectado
 * \return 0 caso OK
 */
int analyse_arguments(int argc, const char *argv[], server *r_server)
{
  char *endptr = NULL;
  int arg_len = 0;

  if (argc < 3)
    return -1;

  if ((arg_len = strlen(argv[2])) >= PORT_LEN)
    return -1;
  r_server->listen_port = strtol(argv[2], &endptr, PORT_NUMBER_BASE);
  arg_len = strlen(argv[2]);
  if(endptr - argv[2] < arg_len)
    return -1;

  if ((arg_len = strlen(argv[1])) >= ROOT_LEN)
    return -1;

  if (0 != access(argv[1], F_OK))
    return -1;
  strncpy(r_server->serv_root, argv[1], ROOT_LEN - 1);

  return 0;
}

/*! \brief Cria o socket para escuta em porta passada como parametro
 *
 * \param[in] r_server Estrutura do servidor, para usar a porta de escuta
 * \param[in] listen_backlog Numero de conexoes em espera
 *
 * \return -1 caso ocorra algum erro
 * \return socket caso esteja ok
 */
int create_listen_socket(const server *r_server, int listen_backlog)
{
  struct sockaddr_in servaddr;
  int listen_socket;
  int enabled = 1;

  listen_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_socket < 0)
    return -1;
  
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(r_server->listen_port);

  setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enabled,
             sizeof(int));
  
  if (bind(listen_socket, (struct sockaddr *)&servaddr,
      sizeof(servaddr)) == -1)
    goto error;

  if (listen(listen_socket, listen_backlog) == -1)
    goto error;

  return listen_socket;

error:
  close(listen_socket);
  return -1;
}

/*! \brief Aceita novas conexoes e aloca no vetor de clientes
 *
 * \param[in] r_server A estrutura servidor para a conexao de um cliente
 *
 * \return -1 caso aconteça algum erro
 * \return 0 caso OK
 */
int make_connection(server *r_server)
{
  int connfd = -1;
  struct sockaddr_in cliaddr;
  socklen_t cli_length = sizeof(cliaddr);
  
  if (r_server->list_of_clients.size == FD_SETSIZE)
    return -1;

  connfd = accept(r_server->listenfd, (struct sockaddr *) &cliaddr, 
                  &cli_length); 
  if (connfd < 0)
    return -1;

  if (add_client_to_list(connfd, &r_server->list_of_clients) < 0)
  {
    close(connfd);
    return -1;
  }

  r_server->maxfd_number = MAX(connfd, r_server->maxfd_number);

  return 0;
}

/*! \brief Fecha a conexao com um cliente 
 *
 * \param[in] client Endereco do cliente atual da lista
 * \param[out] list_of_clients Lista de clientes 
 */
int close_client_connection(client_node **client, client_list
                            *list_of_clients) 
{
  int sockfd = (*client)->sockfd;

  /* move a lista para nao perder a referencia */
  *client = (*client)->next;

  if (remove_client_from_list(sockfd, list_of_clients) < 0)
    return -1;

  return 0;
}

/*! \brief Inicializa a estrutura r_server
 *
 * \param[out] r_server A estrutura a ser inicializada
 *
 */
void init_server(server *r_server)
{
  memset(r_server, 0, sizeof(*r_server));
  r_server->maxfd_number = -1;
}

/*! \brief Inicializa os fd_sets e a referencia do maior descritor
 *
 * \param[out] r_server Estrutura do servidor
 */
void init_sets(server *r_server)
{
  int cur_sockfd = 0;
  client_node *cur_client = NULL;

  FD_ZERO(&r_server->sets.read_s);
  FD_ZERO(&r_server->sets.write_s);
  FD_ZERO(&r_server->sets.except_s);
  FD_SET(r_server->listenfd, &r_server->sets.read_s);
  r_server->maxfd_number = r_server->listenfd;

  for (cur_client = r_server->list_of_clients.head; cur_client; 
       cur_client = cur_client->next)
  {
    cur_sockfd = cur_client->sockfd;
    r_server->maxfd_number = MAX(cur_sockfd, r_server->maxfd_number);

    if(!(cur_client->flags & REQUEST_READ))
      FD_SET(cur_sockfd, &r_server->sets.read_s);
    else
      FD_SET(cur_sockfd, &r_server->sets.write_s);

    FD_SET(cur_sockfd, &r_server->sets.except_s);
  }
}

/*! \brief Funcao que testa se ha buffer alocado, aloca caso 
 * necessario, e recebe uma mensagem (ou parte dela)
 *
 * \param[ou] cur_client A estrutura do cliente
 *
 * \return -1 Caso erro
 * \return 0 Caso ok
 *
 * \notes E' feito um calloc do buffer
 */
int read_client_input(client_node *cur_client)
{
  int n_bytes = 0;

  if (cur_client->buffer == NULL)
  {
    cur_client->buffer = (char *) calloc(BUFFER_LEN, sizeof(char));
    if (cur_client->buffer == NULL)
      return -1;
  }
   
  if (cur_client->pos_buf == BUFFER_LEN - 1)
    return -1;

  if ((n_bytes = recv(cur_client->sockfd, cur_client->buffer +
                      cur_client->pos_buf,
                      BUFFER_LEN - cur_client->pos_buf - 1,
                      MSG_DONTWAIT)) < 0)
  {  
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
      return 0;
    
    return -1;
  }
    
  cur_client->pos_buf += n_bytes;
  return 0; 
}

/*! \brief Verifica entrada de dados do client e determina quando a 
 *  mensagem chegou ao fim
 *
 * \param[out] cur_client A estrutura de _client
 */
int recv_client_msg (client_node *cur_client)
{ 
  if (read_client_input(cur_client) < 0)
    return -1;

  if (verify_double_line(cur_client->buffer) == 0)
    cur_client->flags = cur_client->flags & (~REQUEST_READ);
  else
    cur_client->flags = cur_client->flags | REQUEST_READ;

  return 0; 
}

/* \brief Verifique a mensagem recebida de um cliente, analisando o metodo,
 * o procotolo e o recurso solicitado
 *
 * \param[in] serv_root Caminho do root do servidor
 * \param[out] cur_client Contem informacoes a respeito do cliente
 */
void verify_request(char *serv_root, client_node *cur_client)
{
  char method[METHOD_LEN + 1];
  char resource[RESOURCE_LEN + 1];
  char protocol[PROTOCOL_LEN + 1];

  extr_req_params(cur_client, method, resource, protocol);  
  verify_cli_protocol(protocol, cur_client);
  verify_cli_method(method, cur_client);
  verify_cli_resource(resource, serv_root, cur_client);
  
  memset(cur_client->buffer, 0, sizeof(*cur_client->buffer));
  cur_client->pos_buf = 0;
}

/*! \brief Funcao que gera a resposta ao cliente e armazena em um buffer
 *
 * \param[out] cur_client Estrutura que contem o buffer e informacoes do cliente 
 * (o buffer deve estar inicializado)
 *
 * \return 0 Caso OK
 * \return -1 Caso algum erro
 */
int build_response(client_node *cur_client)
{
  int bytes_read = 0;

  /*! Nao ha necessidade de escrever no buffer atual - sera
   * necessario enviar novamente
   */
  if (cur_client->flags & PENDING_DATA)
    return 0;

  if (!cur_client->pos_buf)
  {
    if (create_header(cur_client) < 0)
      return -1;
  }
  else
    cur_client->pos_buf = 0;

  if (cur_client->resp_status == OK)
  {
    bytes_read = fread(cur_client->buffer + cur_client->pos_buf, 
                       sizeof(char), BUFFER_LEN - cur_client->pos_buf, 
                       cur_client->file);
    if (bytes_read == 0 || ferror(cur_client->file) != 0)
      return -1;
  }

  if (bytes_read < BUFFER_LEN - cur_client->pos_buf)
    cur_client->flags = cur_client->flags & (~REQUEST_READ);

  cur_client->pos_buf += bytes_read;
  return 0;
}

/*! \brief Manda uma resposta para um cliente conectado atraves de um buffer
 * armazenado
 *
 * \param[in] cur_client Variavel que armazena informacoes do cliente
 *
 * \return 0 Caso OK
 * \return -1 Caso haja erro
 */
int send_response(client_node *cur_client)
{
  int sent_bytes = 0;

  if((sent_bytes = send(cur_client->sockfd, cur_client->buffer, 
                        cur_client->pos_buf, MSG_DONTWAIT)) < 0)
  {
    if (errno == EINTR || errno == EAGAIN || errno ==
    EWOULDBLOCK)
    {
      cur_client->flags = cur_client->flags | PENDING_DATA;
      return 0;
    }
    
    return -1;
  }

  cur_client->flags = cur_client->flags & (~PENDING_DATA);
  return 0;
}

