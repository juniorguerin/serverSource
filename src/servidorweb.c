/*!
 *  \file servidorweb.c
 *  \brief Servidor com I/O nao bloqueante que utiliza estrategia 
 *  simplificada de token-bucket para controle de velocidade
 */

#include "server.h"

static volatile int shut_down = 0;
static volatile int alter_config_var = 0;

static void sig_handler (int sig)
{
  if (SIGHUP == sig)
    alter_config_var = 1;
  else
    shut_down = 1;
}

int prepare_signal_handler(struct sigaction *act, sigset_t *mask,
                           sigset_t *orig_mask)
{
  memset(act, 0, sizeof(*act));
  act->sa_handler = sig_handler;

  if (sigaction(SIGTERM, act, 0))
    return -1;

  if (sigaction(SIGINT, act, 0))
    return -1;

  if (sigaction(SIGHUP, act, 0))
    return -1;

  sigemptyset(mask);
  sigaddset(mask, SIGTERM);
  sigaddset(mask, SIGINT);
  sigaddset(mask, SIGHUP);

  if (0 > sigprocmask(SIG_BLOCK, mask, orig_mask))
    return -1;

  return 0;
}

int main(int argc, const char **argv)
{
  server r_server;
  sigset_t mask;
  sigset_t orig_mask;
  struct sigaction act;

  if (0 > prepare_signal_handler(&act, &mask, &orig_mask) ||
      0 > server_init(argc, argv, &r_server))
  {
    fprintf(stderr, "init_error\n");
    goto finish_server;
  }

  while (1)
  {
    client_node *cur_client = NULL;
    int nready = 0;
    struct timespec *timeout = NULL;
    struct timespec burst_rem_time;

    if (0 > server_select_analysis(&r_server, &timeout,
                                   &burst_rem_time))
      goto finish_server;

    nready = pselect(r_server.maxfd_number + 1,
                     &r_server.sets.read_s, &r_server.sets.write_s,
                     &r_server.sets.except_s, timeout, &orig_mask);
    if (shut_down)
      goto finish_server;
    else if (alter_config_var)
    {
      alter_config_var = 0;
      alter_config(&r_server);
    }
    else if (0 > nready)
    {
      if (EINTR == errno)
        continue;

      goto finish_server;
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
          server_client_remove(&cur_client, &r_server);
          continue;
        }

        if (0 != server_recv_response(cur_client) ||
            0 != server_process_write_file(cur_client, &r_server))
        {
          server_client_remove(&cur_client, &r_server);
          continue;
        }

        if (0 >= --nready)
          break;
      }
     
      if (FD_ISSET(sockfd, &r_server.sets.write_s))
      {
        if (0 != server_build_header(cur_client) ||
            0 != server_send_response(cur_client) ||
            0 != server_process_read_file(cur_client, &r_server))
        {
          server_client_remove(&cur_client, &r_server);
          continue;
        }

        if (cur_client->status & FINISHED)
        {
          server_client_remove(&cur_client, &r_server);
          continue;
        }

        if (0 >= --nready)
          break;
      }

      if (FD_ISSET(sockfd, &r_server.sets.except_s))
      {
          server_client_remove(&cur_client, &r_server);
          continue;
      }

      cur_client = cur_client->next;
    }
  }

  return 0;

finish_server:
  clean_up_server(&r_server);
  return -1;
}
