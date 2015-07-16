/*!
 *  \file servidorweb.c
 *  \brief Servidor com I/O nao bloqueante que utiliza estrategia 
 *  simplificada de token-bucket para controle de velocidade
 */

#include "server.h"

server r_server; 

int main(int argc, const char **argv)
{  
  if (0 > server_init(&r_server))
  {
    fprintf(stderr, "init_server");
    goto error;
  }

  if (0 > server_parse_arguments(argc, argv, &r_server))
  {
    fprintf(stderr, "usage: <root> <port> <velocity>\n");
    goto error;
  }

  if (0 > (r_server.listenfd = server_create_listenfd(&r_server)) ||
      0 > (r_server.l_socket = server_create_local_socket()))
  {
    fprintf(stderr, "%s\n", strerror(errno));
    goto error;
  }
 
  while (1)
  {
    client_node *cur_client = NULL;
    int nready = 0;

    while (!nready)
    {
      struct timeval *timeout = NULL;
      server_select_analysis(&r_server, timeout);
      nready = select(r_server.maxfd_number + 1, 
                      &r_server.sets.read_s, &r_server.sets.write_s, 
                      &r_server.sets.except_s, timeout);

      if (0 > nready)
      {
        if (EINTR == errno)
          continue;

        goto error;
      }
    }

    if (FD_ISSET(r_server.listenfd, &r_server.sets.read_s))
    {
      if (0 > server_make_connection(&r_server))
        continue;

      if (0 >= --nready)
        continue;
    }

    cur_client = r_server.l_clients.head;
    while(cur_client)
    {
      int sockfd = cur_client->sockfd;

      if (FD_ISSET(sockfd, &r_server.sets.read_s))
      {
        if (0 > server_recv_client_msg(cur_client))
        {
          server_client_remove(&cur_client, &r_server.l_clients);
          continue;
        }

        if (cur_client->status & REQUEST_RECEIVED)
          server_verify_request(r_server.serv_root, cur_client);

        if (0 >= --nready)
          break;
      }
     
      if (FD_ISSET(sockfd, &r_server.sets.write_s))
      {
        if (0 != server_send_header(cur_client) ||
            0 != server_send_response(cur_client) ||
            0 != server_process_read_file(cur_client, &r_server))
        {
          server_client_remove(&cur_client, &r_server.l_clients);
          continue;
        }

        if (cur_client->status & FINISHED)
        {
          server_client_remove(&cur_client, &r_server.l_clients);
          continue;
        }

        if (0 >= --nready)
          break;
      }

      if (FD_ISSET(sockfd, &r_server.sets.except_s))
      {
          server_client_remove(&cur_client, &r_server.l_clients);
          continue;
      }

      cur_client = cur_client->next;
    }
  }

  return 0;

error:
  unlink(LSOCK_NAME);
  if (r_server.listenfd)
    close(r_server.listenfd);
  if (r_server.l_socket)
    close(r_server.l_socket);
  threadpool_destroy(&r_server.thread_pool);
  return -1;
}
