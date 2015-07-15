/*!
 *  \file servidorweb.c
 *  \brief Servidor com I/O nao bloqueante que utiliza estrategia 
 *  simplificada de token-bucket para controle de velocidade
 */

#include "server.h"

int main(int argc, const char **argv)
{
  server r_server; 
  
  if (0 > init_server(&r_server))
  {
    fprintf(stderr, "init_server");
    goto error;
  }

  if (0 > parse_arguments(argc, argv, &r_server))
  {
    fprintf(stderr, "usage: <root> <port> <velocity>\n");
    goto error;
  }

  if (0 > (r_server.listenfd = create_listenfd(&r_server)) ||
      0 > (r_server.l_socket = create_local_socket()))
  {
    fprintf(stderr, "%s\n", strerror(errno));
    goto error;
  }
 
  while (1)
  {
    client_node *cur_client = NULL;
    int nready;
    
    if(0 > (nready = select_analysis(&r_server)))
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

    cur_client = r_server.l_clients.head;
    while(cur_client)
    {
      int sockfd = cur_client->sockfd;

      if (FD_ISSET(sockfd, &r_server.sets.read_s))
      {
        if (0 > recv_client_msg(cur_client))
        {
          remove_client(&cur_client, &r_server.l_clients);
          continue;
        }

        if (cur_client->status & REQUEST_RECEIVED)
          verify_request(r_server.serv_root, cur_client);

        if (0 >= --nready)
          break;
      }
     
      if (FD_ISSET(sockfd, &r_server.sets.write_s))
      {
        if (0 != send_header(cur_client) ||
            0 != send_response(cur_client) ||
            0 != process_read_file(cur_client, &r_server))
        {
          remove_client(&cur_client, &r_server.l_clients);
          continue;
        }

        if (cur_client->status & FINISHED)
        {
          remove_client(&cur_client, &r_server.l_clients);
          continue;
        }

        if (0 >= --nready)
          break;
      }

      if (FD_ISSET(sockfd, &r_server.sets.except_s))
      {
          remove_client(&cur_client, &r_server.l_clients);
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
