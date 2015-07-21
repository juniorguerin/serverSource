/*!
 *  \file servidorweb.c
 *  \brief Servidor com I/O nao bloqueante que utiliza estrategia 
 *  simplificada de token-bucket para controle de velocidade
 */

#include "server.h"

server r_server; 

int main(int argc, const char **argv)
{
  if (0 > server_init(argc, argv, &r_server))
  {
    fprintf(stderr, "server_init");
    goto error;
  }

  while (1)
  {
    client_node *cur_client = NULL;
    int nready = 0;
    struct timeval *timeout = NULL;
    struct timeval burst_rem_time;

    server_select_analysis(&r_server, &timeout, &burst_rem_time);
    nready = select(r_server.maxfd_number + 1,
                    &r_server.sets.read_s, &r_server.sets.write_s, 
                    &r_server.sets.except_s, timeout);
    if (0 > nready)
    {
      if (EINTR == errno)
        continue;

      goto error;
    }

    if (FD_ISSET(r_server.l_socket, &r_server.sets.read_s))
    {
      server_recv_thread_signals(&r_server);
      server_process_thread_signals(&r_server); 
      
      if (0 >= --nready)
        continue;
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
        if (0 != server_read_client_request(cur_client) ||
            0 != server_verify_request(&r_server, cur_client))
        {
          server_client_remove(&cur_client, &r_server.l_clients);
          continue;
        }

        if (0 != server_recv_response(cur_client) ||
            0 != server_process_write_file(cur_client, &r_server))
        {
          server_client_remove(&cur_client, &r_server.l_clients);
          continue;
        }
      }
     
      if (FD_ISSET(sockfd, &r_server.sets.write_s))
        if (0 != server_build_header(cur_client) ||
            0 != server_send_response(cur_client) ||
            0 != server_process_cli_status(cur_client, &r_server) ||
            0 != server_process_read_file(cur_client, &r_server))
        {
          server_client_remove(&cur_client, &r_server.l_clients);
          continue;
        }

      if (FD_ISSET(sockfd, &r_server.sets.except_s))
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
