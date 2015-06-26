/*!
 * \file recuperadorweb.c
 * \brief Codigo que recupera um arquivo de uma pagina web, Http 1.0
 *
 * "Id: 1"
 */

#include "client.h"

int main(int argc, const char *argv[])
{
  int sockfd = 0;
  int bytes_num = 0;
  struct parsed_url url_info;
  char buffer[BUFFER_LEN];
  char header[HEADER_LEN];
  int overwrite_flag = 0;
  int begin_file = 0; 
  int file_name_pos = 0;
  char http_cod[HTTP_COD_LEN];
  int total_bytes = 0;
  FILE *file = NULL;

  /* inicializacao de variaveis */
  memset(header, 0, sizeof(header));
  memset(buffer, 0, sizeof(buffer));
  memset(http_cod, 0, sizeof(http_cod));
  memset(&url_info, 0, sizeof(struct parsed_url));

  /* testa os argumentos recebidos */
  if (arguments_test(argv, argc, &overwrite_flag, &file_name_pos) == -1)
  {
    fprintf(stderr, "usage: host [nome-file] [flag-file]");
    goto error;
  }

  /* extrai informacoes da url recebida */
  if (get_url_info(argv[1], &url_info, argv, file_name_pos) == -1)
  {
    fprintf(stderr, "url: path/arquivo nao informados\n");
    goto error;
  }

  /* testes sobre o nome do arquivo e sobre a flag */
  if ((file = file_create(url_info.file_name, overwrite_flag)) == NULL)
  {
    fprintf(stderr, "file_create: erro na criacao do arquivo\n");
    goto error;
  }

  /* conexao do socket com o host recebido */
  if (make_connection(&sockfd, url_info.host, url_info.port) == -1)
  {
    fprintf(stderr, "%s\n", strerror(errno));
    goto error;
  }

  /* envia mensagem para o servidor */ 
  if (send_message(url_info.path, sockfd) == -1)
  {
    fprintf(stderr, "message: problema no envio da mensagem\n");
    goto error;
  }

  /* realiza a leitura do header */
  if ((total_bytes = recv_header(header, sockfd)) == -1)
  {
    fprintf(stderr, "recv_header\n");
    goto error;
  }

  /* identifica o codigo http */
  if ((get_http_cod(header, http_cod)) == -1)
  {
    fprintf(stderr, "get_http_cod: erro\n");
    goto error;
  }
  if (*http_cod != '2')
  {
    fprintf(stderr, "%s\n", http_cod);
    goto error;
  }

  /* identifica comeco do arquivo */
  if ((begin_file = ident_begin_file(header)) == -1)
  {
    fprintf(stderr, "ident_begin_file\n");
    goto error;
  }

  /* escreve o restante do buffer atual */
  fwrite(header + begin_file, sizeof(char), total_bytes - begin_file, file);

  /* escreve os outros buffers na integra */
  while ((bytes_num = recv(sockfd, buffer, BUFFER_LEN, 0)) > 0)
    fwrite(buffer, sizeof(char), bytes_num, file);

  /* encerramento e liberacao das variaveis */
  if (close(sockfd))
    fprintf(stderr, "close socket\n");

  if (fclose(file))
    fprintf(stderr, "close file\n");
  
  return 0;

  /* trecho para caso haja erro */
error:
  if (remove(url_info.file_name) != 0)
    fprintf(stderr, "remove");

  if (close(sockfd))
    fprintf(stderr, "close socket\n");

  if (fclose(file))
    fprintf(stderr, "close file\n");

  return -1;
}
