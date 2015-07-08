/*!
 *  \file servidorweb.c
 *  \brief Servidor com soquets nao bloqueantes
 */

#include "server.h"

int main(int argc, const char **argv)
{
  server r_server;
  init_server(&r_server);

  if (0 > analyse_arguments(argc, argv, &r_server))
  {
    fprintf(stderr, "usage: <root> <port> <velocity>\n");
    return -1;
  }

  if (0 > (r_server.listenfd = create_listen_socket(&r_server,
          LISTEN_BACKLOG)))
  {
    fprintf(stderr, "%s\n", strerror(errno));
    return -1;
  }
  
  while (1)
  {
    int nready = 0;
    client_node *cur_client = NULL;
    struct timeval cur_time;
  
    cur_time = burst_init(&r_server.last_burst, 
                          &r_server.list_of_clients);
    if (!init_sets(&r_server))
    {
      sleep_diff_burst(&cur_time, &r_server.last_burst, 1);
      continue;
    }

    if (0 > (nready = select(r_server.maxfd_number + 1, 
        &r_server.sets.read_s, &r_server.sets.write_s, 
        &r_server.sets.except_s, NULL)))
    {
      if (EINTR == errno)
        continue;
     
      close(r_server.listenfd);
      return -1;
    }

    if (FD_ISSET(r_server.listenfd, &r_server.sets.read_s))
    {
      if (0 > make_connection(&r_server))
        continue;

      if (0 >= --nready)
        continue;
    }

    cur_client = r_server.list_of_clients.head;
    while(cur_client)
    {
      int sockfd;

      sockfd = cur_client->sockfd;

      if (FD_ISSET(sockfd, &r_server.sets.read_s))
      {
        if (0 > recv_client_msg(cur_client))
        {
          remove_client(&cur_client, &r_server.list_of_clients);
          continue;
        }

        if (cur_client->flags & REQUEST_READ)
          verify_request(r_server.serv_root, cur_client);

        if (0 >= --nready)
          break;
      }
     
      if (FD_ISSET(sockfd, &r_server.sets.write_s))
      {
        if (0 != build_response(cur_client) || 
            0 != send_response(cur_client))
        {
          remove_client(&cur_client, &r_server.list_of_clients);
          continue;
        }

        if (!(cur_client->flags & REQUEST_READ))
        {
          remove_client(&cur_client, &r_server.list_of_clients);
          continue;
        }
      }

      if (FD_ISSET(sockfd, &r_server.sets.except_s))
      {
          remove_client(&cur_client, &r_server.list_of_clients);
          continue;
      }

      cur_client = cur_client->next;
    }
  }

  close(r_server.listenfd);
  return 0;
}

