/*!
 *  \file servidorweb.c
 *  \brief Servidor com I/O nao bloqueante que utiliza estrategia 
 *  simplificada de token-bucket para controle de velocidade
 */

#include "server.h"

server r_server; 

void sig_handler(int signo)
{
  unlink(r_server.lsocket_name);
  if (r_server.listenfd)
    close(r_server.listenfd);
  if (r_server.l_socket)
    close(r_server.l_socket);
  threadpool_destroy(r_server.thread_pool);

  fprintf(stderr, "Signal %d", signo);
  exit(1);
}

int main(int argc, const char **argv)
{
  signal(SIGINT, sig_handler);
  
  init_server(&r_server);

  if (0 > analyse_arguments(argc, argv, &r_server))
  {
    fprintf(stderr, "usage: <root> <port> <velocity>\n");
    return -1;
  }

  if (0 > (r_server.listenfd = create_listenfd(&r_server)) ||
      0 > (r_server.l_socket = create_local_socket(&r_server)))
  {
    fprintf(stderr, "%s\n", strerror(errno));
    goto error;
  }

  if(!(r_server.thread_pool = threadpool_create(THREAD_NUM)))
  {
    fprintf(stderr, "threadpool_create");
    goto error;
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
      sleep_burst_diff(&cur_time, &r_server.last_burst);
      continue;
    }

    if (0 > (nready = select(r_server.maxfd_number + 1, 
        &r_server.sets.read_s, &r_server.sets.write_s, 
        &r_server.sets.except_s, NULL)))
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

        if (cur_client->status & REQUEST_RECEIVED)
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

        if (!(cur_client->status & WRITE_DATA))
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
  unlink(r_server.lsocket_name);
  if (r_server.listenfd)
    close(r_server.listenfd);
  if (r_server.l_socket)
    close(r_server.l_socket);
  threadpool_destroy(r_server.thread_pool);
  return -1;
}

