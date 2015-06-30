/*!
 *  \file servidorweb.c
 *  \brief Servidor com soquets nao bloqueantes
 */

#include "server.h"

int main(int argc, const char **argv)
{
  Server server;
  int i = 0;
  int sockfd = 0;

  init_server(&server);

  if (analyse_arguments(argc, argv, &server) == -1)
  {
    fprintf(stderr, "usage: <root> <port>\n");
    return -1;
  }

  if ((server.listenfd = create_listen_socket(&server,
          LISTEN_BACKLOG)) == -1)
  {
    fprintf(stderr, "%s\n", strerror(errno));
    return -1;
  }
  
  while (1)
  {
    init_sets(&server);

    int nready = select(server.maxfd_number + 1, &server.sets.read_s, 
        &server.sets.write_s, &server.sets.except_s, NULL);
    if (nready == -1 && errno != EINTR)
    {
      fprintf(stderr, "%s\n", strerror(errno));
      continue;
    }

    if (FD_ISSET(server.listenfd, &server.sets.read_s))
    {
      if (make_connection(&server) == -1)
        continue;

      if (--nready <= 0)
        continue;
    }

    for (i = 0; i <= server.max_cli_index; i++)
    {
      Client *cur_client = &server.Client[i];
      if ((sockfd = cur_client->sockfd) < 0)
        continue;

      if (FD_ISSET(sockfd, &server.sets.read_s))
      {
        if (recv_client_msg(cur_client) != READ_OK)
        {
          close_client_connection(cur_client);
          continue;
        }

        if (cur_client->request_flag == 1)
          if (verify_request(cur_client, server.serv_root) != READ_OK)
            continue;

        if (--nready <= 0)
          break;
      }
     
      if (FD_ISSET(sockfd, &server.sets.write_s))
      {
        if (build_response(cur_client) != 0 || 
            send_response(cur_client) != 0)
        {
          close_client_connection(cur_client);
          continue;
        }

        if (cur_client->request_flag != 1)
        {
          close_client_connection(cur_client);
          continue;
        }
      }

      if (FD_ISSET(sockfd, &server.sets.except_s))
        close_client_connection(cur_client);
    }
  }

  return 0;
}

