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
 * \return end_header Posicao onde encontrou o fim dos headers
 * \return NULL Caso nao encontre o fim dos headers
 */
static char *server_verify_double_line(const char *buffer)
{
  char *end_header = NULL;
  char pattern1[] = {"\r\n\r\n"};
  char pattern2[] = {"\n\n"};

  if ((end_header = strstr(buffer, pattern1)))
    return end_header + strlen(pattern1);
    
  if ((end_header = strstr(buffer, pattern2)))
    return end_header + strlen(pattern2);

  return end_header;
}

/*! \brief Verifica o metodo passado na requisicao do Cliente
 *
 * \param[in] method_str O metodo extraido a partir da requisicao do cliente
 * \param[out] cur_client O cliente em questao 
 */
static void server_verify_cli_method(const char *method_str, 
                                     client_node *cur_client)
{
  int cont = 0;
  int cmp_return = 0;

  do
  {
    cmp_return = strncmp(method_str, supported_methods[cont],
                         strlen(supported_methods[cont]));
    cont++;
  } while (cont < NUM_METHOD && cmp_return);

  if (cmp_return)
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
 */
static void server_verify_cli_resource(const char *resource, 
                                       char *serv_root, 
                                       client_node *cur_client)
{
  char full_path[PATH_MAX];
  char rel_path[PATH_MAX];

  memset(full_path, 0, sizeof(full_path));
  memset(rel_path, 0, sizeof(rel_path));

  strncpy(rel_path, serv_root, ROOT_LEN - 1);
  strncat(rel_path, "/", 1);
  strncat(rel_path, resource, PATH_MAX - ROOT_LEN - 1);
  realpath(rel_path, full_path);
  if (strncmp(serv_root, full_path, strlen(serv_root)))
    cur_client->resp_status = FORBIDDEN;
  else
  {
    if (cur_client->method == GET)
    {
      cur_client->file = fopen(full_path, "r");
      if (!cur_client->file)
        cur_client->resp_status = NOT_FOUND;
      else
        cur_client->resp_status = OK;
    }
    else
    {
      cur_client->file = fopen(full_path, "w");
      cur_client->resp_status = OK;
    }
  }
}

/*! \brief Funcao que analisa um codigo http e retorna a string do status
 *
 * \param[in] http_code Variavel que armazena o codigo http para a resposta
 * ao cliente
 *
 * \return Char correspondente ao codigo
 */
static char *server_http_code_char(const http_code http_code)
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

/*! \brief Verifica qual o protocolo da request do cliente
 *
 * \param[in] protocol O protocolo extraido da request do cliente
 * \param[out] cur_client O cliente em questao
 */
static void server_verify_cli_protocol(const char *protocol, 
                                       client_node *cur_client)
{
  int cont = 0;
  int cmp_return = 0;

  do
  {
    cmp_return = strncmp(protocol, supported_protocols[cont],
                         strlen(supported_protocols[cont]));
    cont++;
  } while (cont < NUM_PROTOCOL && cmp_return);

  if (cmp_return)
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
static void server_extr_req_params(client_node *cur_client, char *method, 
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
  
  if (3 != num_read)
    cur_client->resp_status = BAD_REQUEST;
}

/*! \brief Inicializa os fd_sets e a referencia do maior descritor
 *
 * \param[out] r_server Estrutura do servidor
 */
static int server_init_sets(server *r_server)
{
  int cur_sockfd = 0;
  int transmission = 0;
  client_node *cur_client = NULL;

  FD_ZERO(&r_server->sets.read_s);
  FD_ZERO(&r_server->sets.write_s);
  FD_ZERO(&r_server->sets.except_s);
  FD_SET(r_server->listenfd, &r_server->sets.read_s);
  r_server->maxfd_number = r_server->listenfd;
  FD_SET(r_server->l_socket, &r_server->sets.read_s);
  r_server->maxfd_number = MAX(r_server->maxfd_number,
                               r_server->l_socket);

  for(cur_client = r_server->l_clients.head; cur_client; 
      cur_client = cur_client->next)
  {
    if (!cur_client->bucket.transmission ||
        cur_client->status & SIGNAL_WAIT)
      continue;

    cur_sockfd = cur_client->sockfd;
    r_server->maxfd_number = MAX(cur_sockfd, r_server->maxfd_number);
   
    if (cur_client->status & WRITE_DATA)
      FD_SET(cur_sockfd, &r_server->sets.write_s);
    else
      FD_SET(cur_sockfd, &r_server->sets.read_s);

    FD_SET(cur_sockfd, &r_server->sets.except_s);

    transmission = 1;
  }

  return transmission;
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
static int server_parse_arguments(int argc, const char *argv[],
                                  server *r_server)
{
  char *endptr = NULL;
  int arg_len = 0;

  if (argc != 4)
    return -1;

  if (ROOT_LEN <= (arg_len = strlen(argv[1])))
    return -1;

  if (access(argv[1], F_OK))
    return -1;
  strncpy(r_server->serv_root, argv[1], ROOT_LEN - 1);

  if (PORT_LEN <= (arg_len = strlen(argv[2])))
    return -1;
  r_server->listen_port = strtol(argv[2], &endptr, NUMBER_BASE);
  if (endptr - argv[2] < arg_len)
    return -1;

  if (VEL_LEN <= (arg_len = strlen(argv[3])))
    return -1;
  r_server->velocity = strtol(argv[3], &endptr, NUMBER_BASE);
  if (endptr - argv[3] < arg_len)
    return -1;

  return 0;
}

/*! \brief Cria o socket para escuta em porta passada como parametro
 *
 * \param[in] r_server Estrutura do servidor, para usar a porta de escuta
 *
 * \return -1 Caso ocorra algum erro
 * \return listen_socket Caso esteja ok
 */
static int server_create_listenfd(const server *r_server)
{
  struct sockaddr_in servaddr;
  int listen_socket;
  int enabled = 1;

  if (0 > (listen_socket = socket(AF_INET, SOCK_STREAM, 0)))
    return -1;

  if (0 > setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enabled,
                     sizeof(int)))
    goto error;

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(r_server->listen_port);

  if (0 > bind(listen_socket, (struct sockaddr *) &servaddr,
      sizeof(servaddr)))
    goto error;

  if (0 > listen(listen_socket, LISTEN_BACKLOG))
    goto error;

  return listen_socket;

error:
  if (0 > listen_socket)
    close(listen_socket);

  return -1;
}

/*! \brief Cria o socket local com base no nome guardado em um define
 *
 * \return -1 Caso ocorra algum erro
 * \return local_listen_socket Caso esteja ok
 */
static int server_create_local_socket()
{
  struct sockaddr_un servaddr;
  int l_socket;

  if (0 > (l_socket = socket(AF_UNIX, SOCK_DGRAM, 0)))
    return -1;

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sun_family = AF_UNIX;
  strcpy(servaddr.sun_path, LSOCK_NAME);

  unlink(LSOCK_NAME);
  if (0 > bind(l_socket, (struct sockaddr *)&servaddr,
               sizeof(servaddr)))
    goto error;

  return l_socket;

error:
  if (0 > l_socket)
    close(l_socket);

  return -1;
}

/* \brief Funcao que atualiza o status do cliente apos enviar dados
 *
 * \param[out] client O cliente atualizado
 */
static void server_update_cli_status(client_node *client)
{
  client->status = client->status & (~PENDING_DATA);

  if (client->status & READ_DATA)
  {
    client->status = 0;
    client->status = client->status | FINISHED;
  }

  if (client->status & WRITE_HEADER)
  {
    client->status = client->status & (~WRITE_HEADER);

    if (client->resp_status != OK)
    {
      client->status = 0;
      client->status = client->status | FINISHED;
    }
  }

  if (client->task_st == (task_status) FINISHED)
  {
    client->status = 0;
    client->status = client->status | FINISHED;
  }
}

/*! \brief Gera o header da resposta ao cliente se for necessario
 *
 * \param[in] cliente Estrutura que contem todas as informacoes sobre o cliente
 * e sobre a requisicao feita
 *
 * \return 0 Caso ok
 * \return -1 Caso haja algum erro ou seja status de erro na resposta
 */
int server_build_header(client_node *cur_client)
{
  int resp_status = 0;
  int printf_return = 0;

  /* Nao e necessario gerar o header */
  if (!(cur_client->status & WRITE_HEADER))
    return 0;

  resp_status = cur_client->resp_status;

  printf_return = snprintf(cur_client->buffer, BUFFER_LEN - 1,
                       "%s %d %s\r\n\r\n", 
                       supported_protocols[cur_client->protocol], 
                       resp_status, 
                       server_http_code_char(cur_client->resp_status));
  if (printf_return >= BUFFER_LEN || printf_return < 0)
    return -1;

  cur_client->pos_buf = printf_return;

  return 0;
}

/*! \brief Aloca um novo elemento da estrutura de clientes
 * 
 * \param[in] sockfd O socket para o cliente
 *
 * \return NULL caso haja erro de alocacao
 * \return client caso o cliente seja alocado
 */
client_node *client_node_allocate(int sockfd)
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
void client_node_free(client_node *client)
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
void client_node_append(client_node *client, client_list *l_clients)
{
  client_node *last_client = NULL;

  if (!l_clients->head)
  {
    l_clients->head = client;
    l_clients->size++;
  }
  else
  {
    for (last_client = l_clients->head; last_client->next; 
         last_client = last_client->next)
      ;
    last_client->next = client;
    client->prev = last_client;
    l_clients->size++;
  }
}

/*! \brief Elimina referencia de um elemento dentro da lsita
 *
 * \param[in] sockfd O socket do cliente a ser removido
 * \param[out] l_clients A lista de clientes
 *
 * \return -1 Caso ocorra algum erro
 * \return 0 Caso ok
 */
int client_node_pop(client_node *client, client_list *l_clients)
{
  if (!client || !l_clients->size)
    return -1;

  if (client == l_clients->head)
    l_clients->head = client->next;
  else
  {
    if (client->next)
      client->next->prev = client->prev;

    if (client->prev)
      client->prev->next = client->next;
  }
 
  l_clients->size--;
  return 0;
}

/*! \brief Aceita novas conexoes e aloca no vetor de clientes
 *
 * \param[in] r_server A estrutura servidor para a conexao de um cliente
 * \param[in] velocity A velocidade de transmissao em kb/s
 *
 * \return -1 caso aconteça algum erro
 * \return 0 caso OK
 */
int server_make_connection(server *r_server)
{
  int connfd = -1;
  struct sockaddr_in cliaddr;
  socklen_t cli_length = sizeof(cliaddr);
  client_node *new_client = NULL;
 
  /* Select nao pode monitoras sockets alem de FD_SETSIZE */
  if (FD_SETSIZE == r_server->l_clients.size)
    return -1;

  connfd = accept(r_server->listenfd, (struct sockaddr *) &cliaddr, 
                  &cli_length); 
  if (0 > connfd)
    return -1;

  if (!(new_client = client_node_allocate(connfd)))
  {
    close(connfd);
    return -1;
  }

  client_node_append(new_client, &r_server->l_clients);
  bucket_init(r_server->velocity, &new_client->bucket);
  new_client->status = READ_REQUEST;

  r_server->maxfd_number = MAX(connfd, r_server->maxfd_number);

  return 0;
}

/*! \brief Remove um cliente da lista (fecha a conexao) 
 *
 * \param[out] cur_cli Endereco do cliente atual da lista
 * \param[out] l_clients Lista de clientes
 *
 * \return -1 Caso haja erro
 * \return 0 Caso OK
 *
 * \note Avança ao proximo cliente com o primeiro parametro
 */
int server_client_remove(client_node **cur_client, 
                         client_list *l_clients) 
{
  client_node *client_remove = NULL;
  
  /* Passa ao proximo para nao perder a referencia */
  client_remove = *cur_client;
  *cur_client = (*cur_client)->next;

  if(0 > client_node_pop(client_remove, l_clients))
    return -1;
  
  client_node_free(client_remove);

  return 0;
}

/*! \brief Procedimentos para inicializacao do servidor: inicializa variaveis,
 * cria os sockets e inicia o pool de threads
 *
 * \param[in] argc Numero de argumentos recebidos na funcao main
 * \param[in] argv Os argumentos recebidos na chamada da funcao main
 * \param[out] r_server A estrutura a ser inicializada
 *
 * \return 0 Caso ok
 * \return -1 Caso haja algum erro
 */
int server_init(int argc, const char **argv, server *r_server)
{
  memset(r_server, 0, sizeof(*r_server));
  r_server->maxfd_number = -1;

  if (0 > server_parse_arguments(argc, argv, r_server) ||
      0 > (r_server->listenfd = server_create_listenfd(r_server)) ||
      0 > (r_server->l_socket = server_create_local_socket()) ||
      0 > threadpool_init(LSOCK_NAME, &r_server->thread_pool))
    return -1;
  
  return 0;
}

/*! \brief Funcao que recebe a mensagem e coloca em um buffer
 *
 * \param[in] bytes_to_read Quantidade de bytes a serem lidos
 * \param[out] cur_client A estrutura do cliente
 *
 * \return -1 Caso erro
 * \return 0 Caso ok
 *
 * \notes E' feito um calloc do buffer
 */
int server_recv_client_request(int bytes_to_receive,
                               client_node *cur_client)
{
  int bytes_received = 0;

  if (!cur_client->buffer)
  {
    cur_client->buffer = (char *) calloc(BUFFER_LEN, sizeof(char));
    if (!cur_client->buffer)
      return -1;
  }
  
  if (REQUEST_SIZE - 1 == cur_client->pos_buf)
    return -1;

  if (0 > (bytes_received = recv(cur_client->sockfd, 
           cur_client->buffer + cur_client->pos_buf, 
           bytes_to_receive, MSG_DONTWAIT)))
  {  
    if (EINTR == errno || EAGAIN == errno || EWOULDBLOCK == errno)
      return 0;
    
    return -1;
  }
  
  cur_client->pos_buf += bytes_received;
  return 0; 
}

/*! \brief Verifica entrada de dados do client e determina quando a 
 *  mensagem chegou ao fim
 *
 * \param[out] client A estrutura de _client
 */
int server_read_client_request(client_node *client)
{
  int bytes_to_receive;
  char *pos_header;

  if (!(client->status & READ_REQUEST))
    return 0;

  bytes_to_receive = REQUEST_SIZE - client->pos_buf - 1;
  if (0 > server_recv_client_request(bytes_to_receive, client))
    return -1;

  if ((pos_header = server_verify_double_line(client->buffer)))
  {
    client->pos_header = pos_header - client->buffer;
    client->status = client->status & (~READ_REQUEST);
    client->status = client->status | REQUEST_RECEIVED;
  }

  return 0; 
}

/* \brief Faz analise da mensagem para identificar o metodo,
 * o procotolo e o recurso solicitado
 *
 * \param[in] serv_root Caminho do root do servidor
 * \param[out] cur_client Contem informacoes a respeito do cliente
 */
void server_verify_request(char *serv_root, client_node *client)
{
  char method[METHOD_LEN + 1];
  char resource[RESOURCE_LEN + 1];
  char protocol[PROTOCOL_LEN + 1];

  if (client->status & REQUEST_RECEIVED)
  {
    server_extr_req_params(client, method, resource, protocol);
    server_verify_cli_protocol(protocol, client);
    server_verify_cli_method(method, client);
    server_verify_cli_resource(resource, serv_root, client);

    client->status = client->status & (~REQUEST_RECEIVED);

    if (client->method == GET)
      client->status = client->status | WRITE_HEADER | WRITE_DATA;

    if (client->method == PUT)
      client->status = client->status | READ_DATA;
  }
}

/*! \brief Funcao que le o arquivo solicitado pelo cliente
 *
 * \param[out] task Task com informacoes do cliente como argumento 
 *
 */
void server_read_file(void *c_client)
{
  client_node *client = (client_node *) c_client;
  int bytes_read;

  char *buffer = client->buffer;
  FILE *file = client->file;
  int *pos_buf = &client->pos_buf;
  int b_to_transfer = client->b_to_transfer;
  task_status *task_st = &client->task_st;

  if (0 >= (bytes_read = fread(buffer, sizeof(char), 
                               b_to_transfer, file)))
    *task_st = ERROR;
  else 
  {
    *pos_buf = bytes_read;

    if (b_to_transfer > bytes_read) 
      *task_st = FINISHED;
    else
      *task_st = MORE_DATA;
  }
}

/*! \brief Funcao que escreve o arquivo solicitado pelo cliente
 *
 * \param[out] task Task com informacoes do cliente como argumento
 *
 */
void server_write_file(void *c_client)
{
  client_node *client = (client_node *) c_client;
  int bytes_written;
  char *buffer = client->buffer;
  FILE *file = client->file;
  int *pos_buf = &client->pos_buf;
  int *pos_header = &client->pos_header;
  task_status *task_st = &client->task_st;

  bytes_written = fwrite(buffer + *pos_header, sizeof(char),
                         *pos_buf - *pos_header, file);
  if (0 > bytes_written)
    *task_st = ERROR;
  else
  {
    *pos_buf = 0;
    *task_st = MORE_DATA;
    *pos_header = 0;
    }
}

/* \brief Realiza verificacoes para a escrita do arquivo e coloca a tarefa no
 * pool de threads
 *
 * \param[out] client O cliente correspondente
 * \param[out] r_server A estrutura do servidor
 *
 * \return 0 Caso ok
 * \return -1 Caso haja erro ao colocar a tarefa para as threads
 */
int server_process_write_file(client_node *client, server *r_server)
{
  int not_accept_flags = 0;

  not_accept_flags = PENDING_DATA | FINISHED | SIGNAL_WAIT;

  if (client->status & not_accept_flags || !client->pos_buf)
    return 0;

  if(0 != threadpool_add(server_write_file, client,
                         &r_server->thread_pool))
    return -1;

  client->status = client->status | SIGNAL_WAIT;
  return 0;
}

/* \brief Realiza verificacoes para a leitura do arquivo e coloca a tarefa no
 * pool de threads
 *
 * \param[out] client O cliente correspondente
 * \param[out] r_server A estrutura do servidor
 *
 * \return 0 Caso ok
 * \return -1 Caso haja erro ao colocar a tarefa para as threads
 */
int server_process_read_file(client_node *client, server *r_server)
{
  int bytes_to_read;
  int not_accept_flags = 0;

  not_accept_flags = PENDING_DATA | FINISHED | SIGNAL_WAIT |
                     WRITE_HEADER;

  if (client->status & not_accept_flags || !client->bucket.transmission)
    return 0;
  
  bytes_to_read = BUFFER_LEN;
  if (client->bucket.remain_tokens < BUFFER_LEN)
    bytes_to_read = client->bucket.remain_tokens;
  client->b_to_transfer = bytes_to_read;

  if(0 != threadpool_add(server_read_file, client, 
                         &r_server->thread_pool))
    return -1;

  client->status = client->status | SIGNAL_WAIT;
  return 0;
}

/*! \brief Recebe uma mensagem e armazenada em um buffer
*
 * \param[in] cur_client Variavel que armazena informacoes do cliente
 *
 * \return 0 Caso OK
 * \return -1 Caso haja erro
 */
int server_recv_response(client_node *client)
{
  int b_received;
  int b_to_receive;
  int not_accept_flags;

  not_accept_flags = FINISHED | SIGNAL_WAIT | WRITE_HEADER;

  if (client->status & not_accept_flags || !client->bucket.transmission ||
      client->pos_buf)
    return 0;

  b_to_receive = BUFFER_LEN;
  if (client->bucket.remain_tokens < BUFFER_LEN)
    b_to_receive = client->bucket.remain_tokens;
  client->b_to_transfer = b_to_receive;

  if(0 > (b_received = recv(client->sockfd, client->buffer, b_to_receive,
                            MSG_NOSIGNAL | MSG_DONTWAIT)))
  {
    if (EINTR == errno || EAGAIN == errno || EWOULDBLOCK == errno)
    {
      client->status = client->status | PENDING_DATA;
      return 0;
    }

    return -1;
  }

  bucket_withdraw(b_received, &client->bucket);
  client->status = client->status & (~PENDING_DATA);
  client->pos_buf = b_received;

  if (b_received < b_to_receive)
  {
    client->status = client->status | WRITE_DATA;
    client->status = client->status | WRITE_HEADER;
  }

  return 0;
}

/*! \brief Manda uma resposta armazenada em um buffer  para um cliente
 *
 * \param[in] cur_client Variavel que armazena informacoes do cliente
 *
 * \return 0 Caso OK
 * \return -1 Caso haja erro
 */
int server_send_response(client_node *client)
{
  int b_sent;

  if (client->status & SIGNAL_WAIT ||
      client->status & FINISHED)
    return 0;

  if(0 > (b_sent = send(client->sockfd, client->buffer, 
                        client->pos_buf, MSG_NOSIGNAL | 
                        MSG_DONTWAIT)))
  {
    if (EINTR == errno || EAGAIN == errno || EWOULDBLOCK == errno)
    {
      client->status = client->status | PENDING_DATA;
      return 0;
    }
    
    return -1;
  }

  bucket_withdraw(b_sent, &client->bucket);
  client->pos_buf = 0;
  server_update_cli_status(client);
  return 0;
}

/*! \brief Recebe as mensagens de servico das threads
 *
 * \param[out] r_server A estrutura do servidor
 */
void server_recv_thread_signals(server *r_server)
{
  int cont;
  int b_recv;
  char signal_str[SIGNAL_LEN];
  
  memset(signal_str, 0, sizeof(signal_str));
  memset(r_server->cli_signaled, 0, sizeof(r_server->cli_signaled));

  cont = -1;
  while (++cont < FD_SETSIZE)
  {
    b_recv = recv(r_server->l_socket, signal_str, SIGNAL_LEN,
                  MSG_DONTWAIT);

    if (b_recv <= 0)
      break;

    /* Armazena o socket e o status da tarefa */
    if(1 != (sscanf(signal_str, "%p", &r_server->cli_signaled[cont])))
    {
      memset(r_server->cli_signaled[cont], 0,
             sizeof(* r_server->cli_signaled[cont]));
      cont--;
    }
  }
}

/*! \brief Interpreta a resposta de servido das threads e, caso
 * necessario, fecha a conexao com o cliente
 *
 * \param[out] r_server A estrutura do servidor
 */
void server_process_thread_signals(server *r_server)
{
  int cont;

  cont = 0;
  while (r_server->cli_signaled[cont])
  {
    client_node *cur_client = r_server->cli_signaled[cont];
    if (cur_client->task_st == ERROR)
    {
      client_node_pop(cur_client, &r_server->l_clients);
      client_node_free(cur_client);
    }
    else
      cur_client->status = cur_client->status & (~SIGNAL_WAIT);

    cont++;
  }
}

/*! \brief Contem analises e tarefas necessarias ao select: inicio de burst e
 * inicializacao dos descritores. Ha' determinacao de timeout do select, se
 * necessario. Caso contrario, timeout permanece NULL
 *
 * \param[out] r_server A estrutura do servidor
 * \param[out] timeout O timeout a ser aplicado ao select
 */
void server_select_analysis(server *r_server, struct timeval **timeout,
                            struct timeval *burst_rem_time)
{
  int transmission_flag = 0;
  struct timeval burst_cur_time;

  bucket_burst_init(&r_server->last_burst, &burst_cur_time);
  if (!timerisset(&burst_cur_time))
  {
    client_node *cur_client;
    for(cur_client = r_server->l_clients.head; cur_client; 
        cur_client = cur_client->next)  
      bucket_fill(&cur_client->bucket);
  }

  transmission_flag = server_init_sets(r_server);   
  if (r_server->l_clients.size && !transmission_flag)
  {
    bucket_burst_remain_time(&burst_cur_time, burst_rem_time); 
    *timeout = burst_rem_time;
  } 
}
