/*!
 * \file multithread.h
 * \brief Interface com funcoes para pool de threads
 */

#ifndef MULTITHREAD_H
#define MULTITHREAD_H

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define THREAD_NUM 4
#define SIGNAL_LEN 6

typedef enum task_status_
{
  MORE_DATA,
  FINISHED,
  ERROR,
  NUM_TASK_STATUS
} task_status;

typedef enum task_kind_
{
  READ,
  WRITE,
  NUM_TASK_KIND
} task_kind;

typedef struct task_node_ 
{
  void (*function)(void *); /*<! Ponteiro para a funcao da tarefa */
  void *argument; /*<! Argumento passado para a funcao */
  task_status task_st; /*<! Status da tarefa */
  task_kind task_kd; /*<! Tipo de tarefa */
  int b_to_transfer; /*<! Bytes a transferir */
  int sockfd; /*<! Socket do cliente */
  struct task_node_ *next; /*<! Proximo elemento da lista */
  struct task_node_ *prev; /*<! Elemento anterior */
} task_node;

typedef struct task_list_ {
  task_node *head; /*<! Cabeca da lista */
  int size; /*<! Tamanho da lista */
} task_list;

task_node *task_node_alloc(void (*function)(void *), void *argument);

int task_node_free(task_node *node);

void task_node_append(task_node *new_node, task_list *queue);

int task_node_remove(task_node *node, task_list *queue);

typedef struct threadpool_ {
  pthread_mutex_t lock; /*<! Variavel para mutex */
  pthread_cond_t notify; /*<! Variavel para notificar threads */
  pthread_t *threads; /*<! Array de threads */
  task_list *queue; /*<! Array de queue */
  int shut_down; /*<! Flag para encerramento */
  int thread_count; /*<! Numero de threads */
  int queue_size; /*<! Tamanho da queue */
  int l_socket; /*<! Socket local */
  struct sockaddr_un main_t_address; /*<! Endereco thread principal */
} threadpool;

int threadpool_init(const char *lsocket_name, threadpool *pool);

int threadpool_add(void (*function)(void *), void *argument, 
                   threadpool *pool);

int threadpool_destroy(threadpool *pool);

#endif
