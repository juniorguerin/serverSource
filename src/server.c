/*!
 *  \file server.c
 *  \brief Implementacao das funcoes de servidor
 */

#include "server.h"

const int methods_num = 2;
const char *supported_methods[] = {"GET", "PUT"};
const char supported_protocols[] = {"HTTP/1.0"};

/*! \brief Funcao verifica uma linha dupla em um buffer que e' uma string
 * \param[in] buffer Uma string contendo a mensagem
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

/* A FAZER */
static int verify_cli_method(const char *method_str, Known_methods *cli_method)
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

/*! A FAZER */
static FILE *verify_cli_resource(const char *resource, char *serv_root, 
    Http_code *code)
{
  char *full_path;
  FILE *file;

  if (strstr(resource, "../") != NULL)
  {
    *code = FORBIDDEN;
    return NULL;
  }

  // testar caso os dois tenham barra (ou algum metodo pronto, ou supor que
  // vira' certo?)
  full_path = serv_root;
  if (resource[strlen(resource) - 1] == '/' ||
    serv_root[strlen(serv_root) - 1] == '/')
    strcat(full_path, resource);
  else
    strcat(full_path, strcat("/", resource));

  if (access(full_path, F_OK) == -1)
  {
    *code = NOT_FOUND;
    return NULL;
  }

  file = fopen(full_path, "r");
  if (!file)
  {
    *code = INTERNAL_ERROR;
    return NULL;
  }

  return file;
}

/*! \brief Verifica os argumentos passados para o servidor
 * \param[in] argc Numero de argumentos
 * \param[in] argv Argumentos recebidos com informacoes de porta e root
 * \param[out] server Estrutura do servidor, para determinar a porta de escuta
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
  
  if (bind(listen_socket, (struct sockaddr *)&servaddr,
    sizeof(servaddr)) == -1)
    return -1;

  if (listen(listen_socket, listen_backlog) == -1)
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
 * \param[out] client O cliente a ser considerado 
 * \param[out] set O fd_set em que esta o cliente
 */
void close_client_connection(Client *client, fd_set *set)
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
    server->Client[i].sockfd = -1;
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
 * \param[out] server A estrutura do servidor
 * \return enum Read_status_ que lista os possiveis casos de erros
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

/*! \brief Verifica entrada de dados do client e determina quando a mensagem
 * chegou ao fim
 * \param[out] client A estrutura de Client
 */
int verify_client_msg (Client *client)
{
  int read_return = 0;

  if ((read_return = read_client_input(client)) == READ_OK)
    client->request_flag = verify_double_line(client->buffer);

  return read_return;  
}

/* A FAZER */
int verify_request(Client *client, char *serv_root)
{
  char method[METHOD_LEN + 1];
  char resource[RESOURCE_LEN + 1];
  char protocol[PROTOCOL_LEN + 1];
  Http_code code = 0;
  int num_read = 0;

  memset(method, 0, sizeof(method));
  memset(resource, 0, sizeof(resource));
  memset(protocol, 0, sizeof(protocol));
  num_read = sscanf(client->buffer, "%" STR(METHOD_LEN) "[^ ] %"
      STR(RESOURCE_LEN) "[^ ] %" STR(PROTOCOL_LEN) "[^\r\n]", method, 
      resource, protocol);
  if (num_read != 3)
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

  if (strcmp(protocol, supported_protocols) != 0)
  {
    client->resp_status = BAD_REQUEST;
    return WRONG_READ;
  }

  client->resp_status = OK;
  return READ_OK;
}

/*static char http_code_char(Http_code http_code)
{

  return NULL;
}*/

static int create_header(Client *client)
{
  printf("%s",client->buffer);
  return 0;
}

int build_response(Client *client)
{
  FILE *cur_file = NULL;
  int bytes_read = 0;
  // Se der erro ao ler ou enviar, continua fluxo normal (coloca um contador de
  // quantidade de tentativas e fecha a conexao apo's isto)
  
  if (client->bytes_buf == 0)
  {
    if (create_header(client) < 0)
      return -1;
  }
  else
  {
    memset(client->buffer, 0, sizeof(*client->buffer));
    client->bytes_buf = 0;
  }

  cur_file = client->file;
  do
  {
    bytes_read = fread(client->buffer + client->bytes_buf, sizeof(char), 
        BUFFER_LEN - client->bytes_buf, cur_file);
  } while (bytes_read < BUFFER_LEN - client->bytes_buf && bytes_read != 0);

  if (bytes_read == 0)
    return -1;

  return 0;
}

