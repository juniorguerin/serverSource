/*!
 *  \file servidorweb.c
 *  \brief Servidor com I/O nao bloqueante que utiliza estrategia 
 *  simplificada de token-bucket para controle de velocidade
 */

#include "server.h"

int main(int argc, const char **argv)
{
  server r_server;  

  init_server(&r_server);

  if (0 > parse_arguments(argc, argv, &r_server))
  {
    fprintf(stderr, "usage: <root> <port> <velocity>\n");
    goto error;
  }

  if (0 > (r_server.listenfd = create_listenfd(&r_server)))
  {
    fprintf(stderr, "%s\n", strerror(errno));
    goto error;
  }
 
  while (1)
  {
    int nready = 0;
    int transmission_flag = 0;
    client_node *cur_client = NULL;
    struct timeval burst_cur_time;
    struct timeval *select_timeout = NULL;

    burst_cur_time = burst_init(&r_server.last_burst,
                                &r_server.list_of_clients); 
    
    transmission_flag = init_sets(&r_server);
    if (r_server.list_of_clients.size && !transmission_flag)
    {
      struct timeval burst_rem_time = burst_remain_time(&burst_cur_time);
      select_timeout = &burst_rem_time;
    }
    
    nready = select(r_server.maxfd_number + 1, &r_server.sets.read_s,
                      &r_server.sets.write_s, &r_server.sets.except_s,
                      select_timeout);
    
    if (0 > nready)
    {
      if (EINTR == errno)
        continue;
     
      goto error;
    }

    if (FD_ISSET(r_server.listenfd, &r_server.sets.read_s))
    {
      if (0 > make_connection(&r_server))
        continue;

      if (0 >= --nready)
        continue;
    }

    if (!transmission_flag)
      continue;

    cur_client = r_server.list_of_clients.head;
    while(cur_client)
    {
      int sockfd = cur_client->sockfd;

      if (FD_ISSET(sockfd, &r_server.sets.read_s))
      {
        if (0 > recv_client_msg(cur_client))
        {
          remove_client(&cur_client, &r_server.list_of_clients);
          continue;
        }

        if (cur_client->status & REQUEST_RECEIVED)
          verify_request(r_server.serv_root, cur_client);

        if (0 >= --nready)
          break;
      }
     
      if (FD_ISSET(sockfd, &r_server.sets.write_s))
      {
        if (0 != create_header(cur_client) ||
            0 != build_response(cur_client) || 
            0 != send_response(cur_client))
        {
          remove_client(&cur_client, &r_server.list_of_clients);
          continue;
        }

        if (cur_client->status & FINISHED &&
            !(cur_client->status & PENDING_DATA))
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

  return 0;

error:
  if (r_server.listenfd)
    close(r_server.listenfd);
  return -1;
}

