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
client_node *allocate_client_node(int sockfd)
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
 * \param[out] client o cliente a ser removido
 */
void free_client_node(client_node *client)
{
  close(client->sockfd);
  if (client->buffer)
    free(client->buffer);
  if (client->file)
    fclose(client->file);
  free(client);
}

/*! \brief Adiciona um cliente no final da lista de clientes
 *
 * \param[in] client O cliente a ser adicionado
 * \param[out] cli_list O primeiro elemento da lista
 */
void append_client(client_node *client, client_list *list_of_clients)
{
  client_node *last_client = NULL;

  if (!list_of_clients->head)
  {
    list_of_clients->head = client;
    list_of_clients->size++;
  }
  else
  {
    for (last_client = list_of_clients->head; last_client->next; 
         last_client = last_client->next)
      ;
    last_client->next = client;
    list_of_clients->size++;
  }
}

/*! \brief Elimina referencia de um elemento dentro da lsita
 *
 * \param[in] sockfd O socket do cliente a ser removido
 * \param[out] list_of_clients A lista de clientes
 *
 * \return -1 Caso ocorra algum erro
 * \return 0 Caso ok
 */
int pop_client(int sockfd, client_list *list_of_clients)
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
  
  list_of_clients->size--;

  return 0;
}

/*! \brief Verifica os argumentos passados para o servidor
 *
 * \param[in] argc Numero de argumentos
 * \param[in] argv Argumentos recebidos com informacoes de porta e root
 * \param[out] r_server Estrutura do servidor, para determinar a porta de 
 * escuta
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
 * \param[in] velocity A velocidade de transmissao em kb/s
 *
 * \return -1 caso aconteça algum erro
 * \return 0 caso OK
 */
int make_connection(server *r_server, unsigned int velocity)
{
  int connfd = -1;
  struct sockaddr_in cliaddr;
  socklen_t cli_length = sizeof(cliaddr);
  client_node *new_client = NULL;
 
  /* FD_SETSIZE e o limite de descritores para o select */
  if (FD_SETSIZE == r_server->list_of_clients.size)
    return -1;

  connfd = accept(r_server->listenfd, (struct sockaddr *) &cliaddr, 
                  &cli_length); 
  if (0 > connfd)
    return -1;

  if (!(new_client = allocate_client_node(connfd)))
  {
    close(connfd);
    return -1;
  }

  append_client(new_client, &r_server->list_of_clients);
  bucket_init(velocity, &new_client->bucket);
  r_server->maxfd_number = MAX(connfd, r_server->maxfd_number);

  return 0;
}

/*! \brief Remove um cliente da lista (fecha a conexao) 
 *
 * \param[out] cur_cli Endereco do cliente atual da lista
 * \param[out] list_of_clients Lista de clientes
 *
 * \return -1 Caso haja erro
 * \return 0 Caso OK
 *
 * \note Avança ao proximo cliente com o primeiro parametro
 */
int remove_client(client_node **cur_client, 
                  client_list *list_of_clients) 
{
  int sockfd;
  client_node *client_remove = NULL;
  
  sockfd = (*cur_client)->sockfd;

  /* Passa ao proximo para nao perder a referencia */
  client_remove = *cur_client;
  *cur_client = (*cur_client)->next;

  if(0 > pop_client(sockfd, list_of_clients))
    return -1;
  
  free_client_node(client_remove);

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
 *
 * \return 0 Caso nao haja nenhum cliente apto a transmissao
 * \return 1 Caso haja cliente apto a tramissao ou caso nao haja cliente
 * conectado
 */
int init_sets(server *r_server)
{
  int cur_sockfd = 0;
  int transmission = 0;
  client_node *cur_client = NULL;

  FD_ZERO(&r_server->sets.read_s);
  FD_ZERO(&r_server->sets.write_s);
  FD_ZERO(&r_server->sets.except_s);
  FD_SET(r_server->listenfd, &r_server->sets.read_s);
  r_server->maxfd_number = r_server->listenfd;

  /* caso nao haja cliente conectado */
  if (!r_server->list_of_clients.size)
    return 1;

  cur_client = r_server->list_of_clients.head;
  for(cur_client = r_server->list_of_clients.head; cur_client; cur_client =
    cur_client->next)
  {
    cur_sockfd = cur_client->sockfd;
    r_server->maxfd_number = MAX(cur_sockfd, r_server->maxfd_number);

    if (!cur_client->bucket.transmission)
      continue;

    if (!(cur_client->flags & REQUEST_READ))
      FD_SET(cur_sockfd, &r_server->sets.read_s);
    else
      FD_SET(cur_sockfd, &r_server->sets.write_s);

    FD_SET(cur_sockfd, &r_server->sets.except_s);

    transmission = 1;
  }

  return transmission;
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
  int bytes_received = 0;
  int bytes_to_receive = 0;

  if (!cur_client->buffer)
  {
    cur_client->buffer = (char *) calloc(BUFFER_LEN, sizeof(char));
    if (!cur_client->buffer)
      return -1;
  }
  
  if (BUFFER_LEN - 1 == cur_client->pos_buf)
    return -1;

  bytes_to_receive = BUFFER_LEN - cur_client->pos_buf - 1;
  if (0 > (bytes_received = recv(cur_client->sockfd, 
           cur_client->buffer + cur_client->pos_buf, bytes_to_receive, 
           MSG_DONTWAIT)))
  {  
    if (EINTR == errno || EAGAIN == errno || EWOULDBLOCK == errno)
      return 0;
    
    return -1;
  }
  
  bucket_withdraw(bytes_received, &cur_client->bucket);
  cur_client->pos_buf += bytes_received;
  return 0; 
}

/*! \brief Verifica entrada de dados do client e determina quando a 
 *  mensagem chegou ao fim
 *
 * \param[out] cur_client A estrutura de _client
 */
int recv_client_msg (client_node *cur_client)
{ 
  int bytes_to_receive = BUFFER_LEN - cur_client->pos_buf - 1;
  if (0 > bucket_token_status(&cur_client->bucket, 
      bytes_to_receive))
    return 0;

  if (0 > read_client_input(cur_client))
    return -1;

  if (!verify_double_line(cur_client->buffer))
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

  /* nao ha saldo para transmissao */
  if (0 > bucket_token_status(&cur_client->bucket, BUFFER_LEN))
    return 0;

  /*! Nao ha necessidade de escrever no buffer atual */
  if (cur_client->flags & PENDING_DATA)
    return 0;

  if (!cur_client->pos_buf)
  {
    if (0 > create_header(cur_client))
      return -1;
  }
  else
    cur_client->pos_buf = 0;

  if (OK == cur_client->resp_status)
    if (0 >= (bytes_read = fread(cur_client->buffer + cur_client->pos_buf,
                                 sizeof(char), 
                                 BUFFER_LEN - cur_client->pos_buf, 
                                 cur_client->file)))
      return -1;

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

  /* nao ha saldo para transmissao */
  if (0 > bucket_token_status(&cur_client->bucket, 
                              cur_client->pos_buf))
    return 0;
  
  if((sent_bytes = send(cur_client->sockfd, cur_client->buffer, 
                        cur_client->pos_buf, 
                        MSG_NOSIGNAL | MSG_DONTWAIT)) < 0)
  {
    if (EINTR == errno || EAGAIN == errno || EWOULDBLOCK == errno)
    {
      cur_client->flags = cur_client->flags | PENDING_DATA;
      return 0;
    }
    
    return -1;
  }

  bucket_withdraw(sent_bytes, &cur_client->bucket);
  cur_client->flags = cur_client->flags & (~PENDING_DATA);
  return 0;
}

/*! \brief A cada novo burst (1 segundo) preenche todos os tokens
 *
 * \param[in] last_fill O momento da ultima recarga de tokens
 * \param[in] list_of_clients Lista de clientes
 *
 * \retunr O tempo usado como referencia para a burst
 */
struct timeval burst_set(struct timeval *last_fill, 
               const client_list *list_of_clients)
{
  struct timeval cur_time;
  gettimeofday(&cur_time, NULL);
  long time = timeval_subtract(&cur_time, last_fill);

  if (!list_of_clients->size || time >= 1000000)
  {
    client_node *cur_client = NULL;

    *last_fill = cur_time;
    cur_client = list_of_clients->head; 
    
    while (cur_client)
    {
      bucket_fill(&cur_client->bucket);
      cur_client = cur_client->next;
    }
  }

  return cur_time;
}
