/*!
 * \file client.c
 * \brief Codigo fonte das funcoes utilizadas em clientes http
 *
 * "Id: 1"
 */

#include "client.h"

/*! \brief Faz parse de \a url e guarda os campos em \a url_info
 *
 * \param[in] url Url de onde extrairemos informacoes
 * \param[out] url_info Estrutura com os campos da URL
 * \param[in] argv Argumentos recebidos no main com informacao de Url, flag de
 * sobrescrita e/ou nome do arquivo
 * \param[in] file_name_pos Posicao, dentre os argumentos, onde esta o nome de
 * arquivo (se foi passsado como parametro)
 *
 * \return -1 caso algum erro
 * \return 0 caso sucesso
 */
int get_url_info(const char *url, struct parsed_url *url_info, 
    const char *argv[], int file_name_pos)
{
  char *curr_url = NULL;
  char *begin_url = NULL;
  char *end_url = NULL;
  int length = 0;

  if ((curr_url = strdup(url)) == NULL)
    goto get_url_error;

  begin_url = curr_url;
  end_url = curr_url;

  if(strncmp(end_url, "http://", 7) == 0)
    end_url += 7;

  begin_url = end_url;
  while (*end_url != '\0')
  {
    if (*end_url == ':' || *end_url == '/')
      break;
    end_url++;
  }

  length = end_url - begin_url;
  /* verifica se excedeu o tamanho da variavel */
  if (length >= HOST_LEN)
    goto get_url_error;

  strncpy(url_info->host, begin_url, length);
  begin_url = end_url;

  /* caso tenha encontrado uma porta */
  if (*end_url == ':')
  {
    end_url++;
    begin_url = end_url;

    end_url = strchr(end_url, '/');

    /* nao ha path */
    if (end_url == NULL)
      goto get_url_error;

    length = end_url - begin_url;
    /* verifica se excedeu o tamanho da variavel */
    if (length >= PORT_LEN)
      goto get_url_error;
    strncpy(url_info->port, begin_url, length);
    begin_url = end_url;
  }

  if(url_info->port[0] == '\0')
    strcpy(url_info->port, HTTP_PORT);

  length = strlen(begin_url);
  /* verifica se excedeu o tamanho da variavel */
  if (length >= PATH_LEN)
    goto get_url_error;
  strncpy(url_info->path, begin_url, length);
  
  /* nao ha path */
  if (strlen(url_info->path) == 0)
    goto get_url_error;

  /* coloca no file_name ou o nome do path ou o nome passado como
   * argumento 
   */
  if (file_name_pos != 0)
    strcpy(url_info->file_name,argv[file_name_pos]);
  else
    strcpy(url_info->file_name, basename(url_info->path));
  
  free(curr_url);
  return 0;

get_url_error:
  free(curr_url);
  return -1;
}

/*! \brief Realiza testes com os argumentos recebidos
 *
 * \param[in] argv Os parametros do main (URL, flag de sobrescrita e/ou nome 
 * do arquivo)
 * \param[in] argc Numero de parametros recebidos pela funcao no main
 * \param[out] overwrite_flag Flag para verificar sobrescrita de arquivo
 * \param[out] file_name_pos Identifica em qual dos argumentos esta o nome do
 * arquivo
 *
 * \return -1 se a flag de sobrescrita estiver errada
 * \return 0 caso sucesso
 */
int  arguments_test(const char *argv[], int argc, int *overwrite_flag, 
    int *file_name_pos)
{
  /* testes da quantidade de parametros recebidos */
  if ((argc < 2) || (argc > 4))
    return -1;

  if (argc == 3)
  {
    if (strncmp(argv[2], FLAG_WRITE, strlen(FLAG_WRITE)) == 0)
    {
      *overwrite_flag = 1;
      *file_name_pos = 0;
    }
    else
      *file_name_pos = 2;
  }
  else
  {
    if (strncmp(argv[2], FLAG_WRITE, strlen(FLAG_WRITE)) == 0)
    {
      *overwrite_flag = 1;
      *file_name_pos = 3;
    }
    else if (strncmp(argv[3], FLAG_WRITE, strlen(FLAG_WRITE)) == 0)
    {
      *overwrite_flag = 1;
      *file_name_pos = 2;
    }
    else
      return -1;
  }

  return 0;
}

/*! \brief Cria um socket e realiza a conexao com um host
 *
 * \param[in] host O host a se conectar
 * \param[in] port A porta a se conectar
 * \param[out] sockfd O socket 
 *
 * \return -1 se houver algum problema de conexao
 * \return 0 caso sucesso
 */
int make_connection(int *sockfd, const char *host, const char *port)
{
  int addrinfo_code = 0;
  int func_code = 0;
  struct addrinfo *rp = NULL;
  struct addrinfo *servinfo = NULL;
  struct addrinfo hints;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo_code = getaddrinfo(host, port, &hints, &servinfo);

  if( addrinfo_code != 0)
    return -1;

  for (rp = servinfo; rp != NULL; rp = rp->ai_next)
  {
    *sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

    if (*sockfd == -1)
      continue;

    if ((func_code = connect(*sockfd, rp->ai_addr, rp->ai_addrlen))
      != -1)
      break;

    close(*sockfd);
  }
  freeaddrinfo(servinfo);

  if (rp == NULL)
    return -1;

  if (func_code == -1)
    return -1;

  return 0;
}

/*! \brief Cria um arquivo, considerando a flag de sobrescrita 
 *    ou nao;
 *
 * \param[in] file_name Nome do arquivo a ser criado
 * \param[in] overwrite_flag Flag de sobrescrita de arquivo
 *
 *  \return NULL se houver algum erro
 *  \return FILE se nao houver impedimento  
 */
FILE *file_create(const char *file_name, int overwrite_flag)
{
  if ((access(file_name, F_OK) == 0) && (overwrite_flag == 0))
    return NULL;

  return fopen(file_name, "w");
}

/*! \brief Manda mensagem http GET para o host especificado;
 *
 * \param[in] path O caminho do recurso
 * \param[in] sockfd O socket para conexao
 *
 * \return -1 se houver algum problema com o envio/formatacao da mensagem
 * \return 0 caso OK
 */
int send_message(const char *path, int sockfd)
{
  char message[MESSAGE_LEN];
  int msg_bytes = 0;
  int sent_bytes = 0;
  int cont_send = 0;

  msg_bytes = snprintf(message, MESSAGE_LEN,
                       "GET %s HTTP/1.0\r\n\r\n", path);
  if (msg_bytes >= MESSAGE_LEN || msg_bytes < 0) 
    return -1;

  do
  {
    sent_bytes = send(sockfd, message, msg_bytes, 0);
    cont_send++;

    if (sent_bytes == -1 && errno == EINTR)
      usleep(300);

  } while (sent_bytes == -1 && errno == EINTR && cont_send <= LIMIT_SEND);

  if (sent_bytes == -1 || cont_send == LIMIT_SEND)
    return -1;

  return 0;
}

/*! \brief Identifica o codigo http da mensagem 
 *
 * \param[in] header Armazena o header da mensagem
 * \param[out] http_cod Ponteiro para a posicao onde comeca o codigo http no
 * header
 *
 * \return -1 caso haja algum erro
 * \return 0 caso OK
 *
 * \note necessario usar free no segundo parametro
 */
int get_http_cod(const char *header, char *http_cod)
{ 
  char *http_cod_begin;
  char *http_cod_end;
  int http_cod_len = 0;

  /* procura o primeiro branco para evitar o começo do http */
  http_cod_begin = strchr(header, ' ');
  if (http_cod_begin == NULL)
    return -1;
  http_cod_begin++;

  /* procura o primeiro \n */
  http_cod_end = strchr(header, '\n');
  if (http_cod_begin == NULL)
    return -1;
  http_cod_end--;

  http_cod_len = http_cod_end - http_cod_begin;
  if (http_cod_len > HTTP_COD_LEN)
    return -1;
  strncpy(http_cod, http_cod_begin, http_cod_len);

  return 0;
}

/*! \brief Copia no minimo todo o header para um array de caracteres
 *
 * param[out] header Armazena o header da mensagem
 * param[in] sockfd Socket para conexao
 *
 * \return total de bytes armazenados caso OK   
 * \return -1 em caso de erro
 */
int recv_header(char *header, int sockfd)
{
  int bytes_num = 0;
  int cont_pos_header = 0;
  char buffer[HEADER_LEN];

  memset(buffer, 0, sizeof(buffer));
  
  while (cont_pos_header < HEADER_LEN - 1)
  {
    bytes_num = recv(sockfd, buffer, HEADER_LEN - cont_pos_header - 1, 0);

    if (bytes_num <= 0)
      break;

    memcpy(header + cont_pos_header, buffer, bytes_num);
    cont_pos_header += bytes_num;
  }

  if (bytes_num < 0)
    return -1;

  return cont_pos_header;
}

/*! \brief Funcao que identifica onde comeca o arquivo em um header
 *
 * \param[in] header Armazena no minimo todo o header da mensagem
 *
 * \return -1 Caso nao encontre um comeco do arquivo
 * \reutrn A posicao do comeco do arquivo
 */
int ident_begin_file(char *header)
{
  char *cur_header = header;
  int cont_header = 0;

  while (cont_header < HEADER_LEN)
  {
    if ((cur_header = strchr(cur_header, '\n')) == NULL)
      return -1;

    cur_header++;
    cont_header = cur_header - header;
    if(strncmp(cur_header, "\r\n", 2) == 0)
    {
      cur_header += 2;
      cont_header += 2;
      break;
    }

    if(*cur_header == '\n')
    {
      cur_header += 1;
      cont_header += 1;
      break;
    }
  }

  return cont_header;
}
