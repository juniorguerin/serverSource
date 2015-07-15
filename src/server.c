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
  if (!strstr(buffer, "\r\n\r\n") && !strstr(buffer, "\n\n"))
    return 0;
    
  return 1;
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
 *
 * \return 0 Caso a analise chegue ao fim
 * \return -1 Caso se encaixe em algum dos casos de erro
 */
static int verify_cli_resource(const char *resource, char *serv_root, 
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
  if (strcmp(serv_root, full_path) > 0)
  {
    cur_client->resp_status = FORBIDDEN;
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
  
  if (3 != num_read)
    cur_client->resp_status = BAD_REQUEST;
}

/*! \brief Gera o header da resposta ao cliente se for necessario
 *
 * \param[in] cliente Estrutura que contem todas as informacoes sobre o cliente
 * e sobre a requisicao feita
 *
 * \return 0 Caso ok
 * \return -1 Caso haja algum erro ou seja status de erro na resposta
 */
int send_header(client_node *cur_client)
{
  int resp_status = 0;
  int printf_return = 0;

  /* Nao e necessario gerar o header */
  if (cur_client->status & WRITE_DATA ||
      cur_client->status & PENDING_DATA)
    return 0;

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
void append_client(client_node *client, client_list *l_clients)
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
    client->before = last_client;
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
int pop_client(client_node *client, client_list *l_clients)
{
  if (!client || !l_clients->size)
    return -1;

  if (client == l_clients->head)
    l_clients->head = client->next;
  else
  {
    if (client->next)
      client->next->before = client->before;

    if (client->before)
      client->before->next = client->next;
  }
 
  l_clients->size--;
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
int parse_arguments(int argc, const char *argv[], server *r_server)
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
int create_listenfd(const server *r_server)
{
  struct sockaddr_in servaddr;
  int listen_socket;
  int enabled = 1;

  if (0 > (listen_socket = socket(AF_INET, SOCK_STREAM, 0)))
    return -1;
  
  if (0 > setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &enabled,
                     sizeof(int)))
    return -1;
  
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
int create_local_socket()
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

/*! \brief Aceita novas conexoes e aloca no vetor de clientes
 *
 * \param[in] r_server A estrutura servidor para a conexao de um cliente
 * \param[in] velocity A velocidade de transmissao em kb/s
 *
 * \return -1 caso aconteça algum erro
 * \return 0 caso OK
 */
int make_connection(server *r_server)
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

  if (!(new_client = allocate_client_node(connfd)))
  {
    close(connfd);
    return -1;
  }

  append_client(new_client, &r_server->l_clients);
  bucket_init(r_server->velocity, &new_client->bucket);
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
int remove_client(client_node **cur_client, 
                  client_list *l_clients) 
{
  client_node *client_remove = NULL;
  
  /* Passa ao proximo para nao perder a referencia */
  client_remove = *cur_client;
  *cur_client = (*cur_client)->next;

  if(0 > pop_client(client_remove, l_clients))
    return -1;
  
  free_client_node(client_remove);

  return 0;
}

/*! \brief Inicializa a estrutura r_server
 *
 * \param[out] r_server A estrutura a ser inicializada
 *
 * \return 0 Caso ok
 * \return -1 Caso haja erro de iniciacao
 */
int init_server(server *r_server)
{
  memset(r_server, 0, sizeof(*r_server));
  r_server->maxfd_number = -1;

  if (0 > threadpool_init(LSOCK_NAME, &r_server->thread_pool))
    return -1;
  
  return 0;
}

/*! \brief Inicializa os fd_sets e a referencia do maior descritor
 *
 * \param[out] r_server Estrutura do servidor
 *
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

  for(cur_client = r_server->l_clients.head; cur_client; 
      cur_client = cur_client->next)
  {
    if (!cur_client->bucket.transmission)
      continue;

    cur_sockfd = cur_client->sockfd;
    r_server->maxfd_number = MAX(cur_sockfd, r_server->maxfd_number);
    
    if (cur_client->status & WRITE_HEADER || 
        cur_client->status & WRITE_DATA)
      FD_SET(cur_sockfd, &r_server->sets.write_s);
    else if(!(cur_client->status & SIGNAL_READY))
      FD_SET(cur_sockfd, &r_server->sets.read_s);

    FD_SET(cur_sockfd, &r_server->sets.except_s);

    transmission = 1;
  }

  return transmission;
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
int read_client_input(int bytes_to_receive, client_node *cur_client)
{
  int bytes_received = 0;

  if (!cur_client->buffer)
  {
    cur_client->buffer = (char *) calloc(BUFFER_LEN, sizeof(char));
    if (!cur_client->buffer)
      return -1;
  }
  
  if (BUFFER_LEN - 1 == cur_client->pos_buf)
    return -1;

  if (0 > (bytes_received = recv(cur_client->sockfd, 
           cur_client->buffer + cur_client->pos_buf, 
           bytes_to_receive, MSG_DONTWAIT)))
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
int recv_client_msg(client_node *cur_client)
{
  int bytes_to_receive;

  bytes_to_receive = BUFFER_LEN - cur_client->pos_buf - 1;
  if (cur_client->bucket.rate < bytes_to_receive)
    bytes_to_receive = cur_client->bucket.remain_tokens;
  
  if (0 > read_client_input(bytes_to_receive, cur_client))
    return -1;

  if (!verify_double_line(cur_client->buffer))
    cur_client->status = cur_client->status & (~REQUEST_RECEIVED);
  else
    cur_client->status = cur_client->status | REQUEST_RECEIVED;

  return 0; 
}

/* \brief Faz analise da mensagem para identificar o metodo,
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

  cur_client->status = cur_client->status & (~REQUEST_RECEIVED);
  cur_client->status = cur_client->status | WRITE_HEADER;
}

/*! \brief Funcao que le o arquivo solicitado pelo cliente
 *
 * \param[out] task Task com informacoes do cliente como argumento 
 *
 */
void read_file(void *c_args)
{
  io_args *args = (io_args *) c_args;
  int bytes_read;
 
  if (0 >= (bytes_read = fread(args->buffer,
                               sizeof(char), 
                               args->b_to_transfer, 
                               args->file)))
    args->task_status = -1;
  else
  { 
    args->b_transferred = bytes_read;
    args->task_status = 1;
  }
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
int process_read_file(client_node *client, server *r_server)
{
  io_args args;
  int bytes_to_read; 

  if (client->status & PENDING_DATA ||
      !(client->status & WRITE_DATA))
    return 0;
  
  if (BUFFER_LEN <= client->bucket.remain_tokens) 
    bytes_to_read = BUFFER_LEN;
  else
    bytes_to_read = client->bucket.remain_tokens;
  
  args.file = client->file;
  args.buffer = client->buffer;
  args.sockfd = client->sockfd;
  args.b_to_transfer = bytes_to_read;

  if(0 != threadpool_add(read_file, &args, &r_server->thread_pool))
    return -1;

  r_server->wait_signal++;
  return 0;
}

/*! \brief Manda uma resposta armazenada em um buffer  para um cliente
 *
 * \param[in] cur_client Variavel que armazena informacoes do cliente
 *
 * \return 0 Caso OK
 * \return -1 Caso haja erro
 */
int send_response(client_node *client)
{
  int b_sent;

  if (!(client->status & WRITE_DATA) && 
      !(client->status & WRITE_HEADER) &&
      !(client->status & PENDING_DATA))
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
  client->status = client->status & (~PENDING_DATA);
  client->status = client->status & (~WRITE_DATA);

  if (client->status & WRITE_HEADER)
  {
    if (client->resp_status != OK)
      client->status = client->status | FINISHED;
    else
    {
      client->status = client->status & (~WRITE_HEADER);
      client->status = client->status | WRITE_DATA;
    }
  }
  
  return 0;
}

/*! \brief Recarrega todos os buckets a cada 1 segundo
 *
 * \param[in] last_fill O momento da ultima recarga de tokens
 * \param[in] l_clients Lista de clientes
 *
 * \return Tempo atual da burst em questao
 */
struct timeval burst_init(struct timeval *last_fill, 
               const client_list *l_clients)
{
  struct timeval cur_time;
  struct timeval burst_cur_time;

  gettimeofday(&cur_time, NULL);
  burst_cur_time = timeval_subtract(&cur_time, last_fill);

  if (burst_cur_time.tv_sec >= 1)
  {
    client_node *cur_client = NULL;

    *last_fill = cur_time;
    memset(&burst_cur_time, 0, sizeof(burst_cur_time));

    for(cur_client = l_clients->head; cur_client; 
        cur_client = cur_client->next)  
      bucket_fill(&cur_client->bucket);
  }

  return burst_cur_time;
}

/*! \brief Recebe as mensagens de servico das threads
 *
 * \param[out] r_server A estrutura do servidor
 *
 */
void recv_thread_signals(server *r_server)
{
  int cont;
  int b_recv;
  char signal_str[SIGNAL_LEN];
  char *endptr = NULL;
  socklen_t address_len;
  
  memset(signal_str, 0, sizeof(signal_str));
  memset(r_server->signals, 0, sizeof(r_server->signals));
  address_len = sizeof(r_server->thread_pool.main_t_address);

  cont = -1;
  while (++cont < SIGNAL_MAX)
  {
    b_recv = recvfrom(r_server->l_socket, signal_str, SIGNAL_LEN,
                      MSG_DONTWAIT, (struct sockaddr *) 
                      &r_server->thread_pool.main_t_address,
                      &address_len);

    if (b_recv <= 0)
      break;

    /* Armazena o socket e o status da tarefa */
    r_server->signals[cont][0] = strtol(strtok(signal_str, " "), &endptr,
                              NUMBER_BASE);
    r_server->signals[cont][1] = strtol(strtok(NULL, "/0"), &endptr,
                              NUMBER_BASE);

    r_server->wait_signal--;
  }
}

/*! \brief Interpreta a resposta de servido das threads e, caso
 * necessario, fecha a conexao com o cliente
 *
 * \param[out] r_server A estrutura do servidor
 *
 */
void process_thread_signals(server *r_server)
{
  int cont;
  client_node *cur_client = NULL;

  cont = 0;
  while (r_server->signals[cont][0] != 0)
  {
    int sockfd = r_server->signals[cont][0];
    int status = r_server->signals[cont][1];

    cur_client = r_server->l_clients.head;
    while (sockfd != cur_client->sockfd && !cur_client)
      cur_client = cur_client->next;

    if (!cur_client)
      continue;

    if (status < 0)
      remove_client(&cur_client, &r_server->l_clients);
    else
    {
      cur_client->status = cur_client->status | WRITE_DATA;
      cur_client->status = cur_client->status | SIGNAL_READY;
    }

    cont++;
  }
}

/*! \brief Contem analises e tarefas necessarias ao inicio de cada laco do
 * select: analise de sinalizacao das threads, calculo dos tempos de burst,
 * e inicializacao dos conjuntos de descritores
 *
 * \param[out] r_server A estrutura do servidor
 *
 * \return 0 Caso ok
 * \return -1 Caso haja erro no select
 */
int select_analysis(server *r_server)
{
  int nready = 0;
  int transmission_flag = 0;
  struct timeval *timeout;
  struct timeval burst_cur_time;
  struct timeval burst_rem_time;
  struct timeval timeout_ref;

  /* Tempo de espera por sinalizacao */
  timeout_ref.tv_sec = 0;
  timeout_ref.tv_usec = 1000;

  while (!nready)
  {
    timeout = NULL;
    
    if (0 < r_server->wait_signal)
    {
      recv_thread_signals(r_server);
      process_thread_signals(r_server);
    }

    burst_cur_time = burst_init(&r_server->last_burst,
                                &r_server->l_clients);   
    transmission_flag = init_sets(r_server); 
    
    if (r_server->l_clients.size && !transmission_flag)
    {
      burst_rem_time = burst_remain_time(&burst_cur_time); 

      if (0 < r_server->wait_signal)
        timeout = &timeout_ref;
      else
        timeout = &burst_rem_time;
    }

    nready = select(r_server->maxfd_number + 1, &r_server->sets.read_s,
                    &r_server->sets.write_s, &r_server->sets.except_s,
                    timeout);
  }
 
 return nready;
}
