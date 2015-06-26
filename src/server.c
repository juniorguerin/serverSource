/*!
 *  \file server.c
 *  \brief Implementacao das funcoes de servidor
 */

#include "server.h"

/*! \brief Verifica os argumentos passados para o servidor
 * \param[in] argc Numero de argumentos
 * \param[in] argv Argumentos recebidos com informacoes de porta e root
 * \param[out] server Estrutura do servidor, para determinar a porta de escuta
 * \return -1 caso algum erro tenha sido detectado
 * \return 0 caso OK
 */
int analise_arguments(int argc, const char *argv[], Server *server)
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
 * \param[in] server Estrutura do servidor, para usar a porta de escuta
 * \param[in] listen_backlog Numero de conexoes em espera
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
  
  if(bind(listen_socket, (struct sockaddr *)&servaddr,
    sizeof(servaddr)) == -1)
    return -1;

  if(listen(listen_socket, listen_backlog) == -1)
    return -1;

  return listen_socket;
}

/*! \brief Aceita novas conexoes e aloca no vetor de clientes
 * \param[in] server A estrutura servidor para a conexao de um cliente
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
    if (server->clients[i].sockfd < 0)
    {
      server->clients[i].sockfd = connfd;
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
 * \param[out] client O cliente a ser considerado 
 * \param[out] set O fd_set em que esta o cliente
 */
void close_client_connection(Clients *client, fd_set *set)
{
  FD_CLR(client->sockfd, set);
  close(client->sockfd); 

  memset(client, 0, sizeof(*client));
  client->sockfd = -1;

  if (client->buffer != NULL)
    free(client->buffer);
}

/*! \brief Inicializa a estrutura Server
 * \param[out] server A estrutura a ser inicializada
 */
void init_server(Server *server)
{
  int i = 0;
  memset(server, 0, sizeof(*server));
  server->max_cli_index = -1;
  server->maxfd_number = -1;
  
  for (i = 0; i < FD_SETSIZE; i++)
    server->clients[i].sockfd = -1;
}

/*! \brief Inicializa os fd_sets e a referencia do maior descritor
 * \param[out] server Estrutura do servidor
 */
void init_sets(Server *server)
{
  int i = 0;

  FD_ZERO(&server->sets.read_s);
  FD_ZERO(&server->sets.write_s);
  FD_ZERO(&server->sets.except_s);
  FD_SET(server->listenfd, &server->sets.read_s);
  FD_SET(server->listenfd, &server->sets.write_s);
  server->maxfd_number = server->listenfd;

  for (i = 0; i <= server->max_cli_index; i++)
  {
    int cur_sock = server->clients[i].sockfd;
    if (cur_sock < 0)
      continue;

    server->maxfd_number = MAX(cur_sock, server->maxfd_number);

    if(server->clients[i].write_flag)
      FD_SET(cur_sock, &server->sets.write_s);
    else
      FD_SET(cur_sock, &server->sets.read_s);

    FD_SET(cur_sock, &server->sets.except_s);
  }
}

/*! \brief Funcao que testa se ha buffer alocado, aloca caso 
 * necessario, e recebe uma mensagem (ou parte dela)
 * \param[out] server A estrutura do servidor
 * \return enum Read_status_ que lista os possiveis casos de erros
 * \notes E' feito um calloc do buffer_in de server_clients
 */
int read_client_input(Clients *client)
{
  int n_bytes = 0;

  if (client->buffer == NULL)
  {
    client->buffer = (char *) calloc(BUFFER_LEN, sizeof(char));

    if (client->buffer == NULL)
      return COULD_NOT_ALLOCATE;
  }
   
  if (client->bytes_buf == BUFFER_LEN - 1)
    return BUFFER_OVERFLOW;

  if ((n_bytes = read(client->sockfd, client->buffer +
    client->bytes_buf, BUFFER_LEN - client->bytes_buf - 1)) >  0)
  {
    client->bytes_buf += n_bytes;
    return READ_OK;
  }

  if (n_bytes == 0)
    return ZERO_READ;
    
  return COULD_NOT_READ;
}

/*! \brief Funcao verifica uma linha dupla em um buffer que e' uma string
 * \param[in] buffer Uma string contendo a mensagem
 * \return 1 Caso tenha encontrado o fim da mensagem
 * \return 0 Caso nao tenha encontrado o fim da mensagem
 */
int verify_double_line(char *buffer)
{
  if (strstr(buffer, "\r\n\r\n") == NULL)
    if (strstr(buffer, "\n\n") == NULL)
      return 0;
    
  return 1;
}

/*! \brief Verifica entrada de dados do client e determina quando a mensagem
 * chegou ao fim
 * \param[out] client A estrutura de clients
 */
int verify_client_msg (Clients *client)
{
  int read_return = 0;

  if ((read_return = read_client_input(client)) == READ_OK)
    client->write_flag = verify_double_line(client->buffer);

  return read_return;  
}
