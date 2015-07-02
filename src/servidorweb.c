/*!
 *  \file servidorweb.c
 *  \brief Servidor com soquets nao bloqueantes
 */

#include "server.h"
#include "token-bucket.h"

int main(int argc, const char **argv)
{
  server r_server;
  int i = 0;
  int sockfd = 0;

  init_server(&r_server);

  if (analyse_arguments(argc, argv, &r_server) == -1)
  {
    fprintf(stderr, "usage: <root> <port>\n");
    return -1;
  }

  if ((r_server.listenfd = create_listen_socket(&r_server,
          LISTEN_BACKLOG)) == -1)
  {
    fprintf(stderr, "%s\n", strerror(errno));
    return -1;
  }
  
  while (1)
  {
    int nready = 0;

    init_sets(&r_server);

    if ((nready = select(r_server.maxfd_number + 1, &r_server.sets.read_s, 
        &r_server.sets.write_s, &r_server.sets.except_s, NULL)) < 0)
    {
      if (errno == EINTR)
        continue;
      
      return -1;
    }

    if (FD_ISSET(r_server.listenfd, &r_server.sets.read_s))
    {
      if (make_connection(&r_server) == -1)
        continue;

      if (--nready <= 0)
        continue;
    }

    for (i = 0; i <= r_server.max_cli_index; i++)
    {
      client *cur_client = &r_server.client_list[i];
      if ((sockfd = cur_client->sockfd) < 0)
        continue;

      if (FD_ISSET(sockfd, &r_server.sets.read_s))
      {
        if (recv_client_msg(cur_client) < 0)
        {
          close_client_connection(cur_client);
          continue;
        }

        if (cur_client->flags & REQUEST_READ)
          if (verify_request(cur_client, r_server.serv_root) < 0)
            continue;

        if (--nready <= 0)
          break;
      }
     
      if (FD_ISSET(sockfd, &r_server.sets.write_s))
      {
        if (build_response(cur_client) != 0 || 
            send_response(cur_client) != 0)
        {
          close_client_connection(cur_client);
          continue;
        }

        if (!(cur_client->flags & REQUEST_READ))
        {
          close_client_connection(cur_client);
          continue;
        }
      }

      if (FD_ISSET(sockfd, &r_server.sets.except_s))
        close_client_connection(cur_client);
    }
  }

  return 0;
}

