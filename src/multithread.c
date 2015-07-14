/*!
 * \file multithread.c
 * \brief Implementa interface de funcoes para pool de threads
 */

#include "multithread.h"

/*! \brief Distribui as tarefas entre as threads
 *
 * \param[out] cur_threadpool O pool de threads
 */
static void *threadpool_thread(void *cur_threadpool)
{
  int bytes_sent;
  char signal_str[SIGNAL_LEN];
  threadpool *pool = (threadpool *) cur_threadpool;
  task_node *task = NULL;
  io_args *args = NULL;

  while (1)
  {
    pthread_mutex_lock(&(pool->lock));

    while (!pool->queue->size && !pool->shut_down)
      pthread_cond_wait(&(pool->notify), &(pool->lock));

    if (pool->shut_down)
      break;

    task = pool->queue->head;
    remove_task_f_list(pool->queue->head, pool->queue);

    pthread_mutex_unlock(&(pool->lock));

    (*(task->function))(task->argument);
    args = (io_args *) task->argument;

    sprintf(signal_str, "%d %d", args->sockfd, 
            args->task_status);
    bytes_sent = sendto(pool->l_socket, signal_str, SIGNAL_LEN, 0,
                        (struct sockaddr *) &pool->main_t_address,
                        sizeof(struct sockaddr_un));

    free_task_node(task);
  }
  
  pthread_mutex_unlock(&(pool->lock));
  pthread_exit(NULL);
  return (NULL);
}

/*! \brief Funcao que inicia um pool de threads que ja esteja alocado
 *
 * \param[in] lsocket_name O nome do socket local da thread principal
 * \param[out] pool O pool a ser iniciado
 *
 * \return -1 Caso haja erro
 * \return 0 Caso ok
 */
int threadpool_init(const char *lsocket_name, threadpool *pool) 
{
  int i;

  if (!pool) 
    return -1;

  pool->threads = (pthread_t *) calloc(THREAD_NUM, sizeof(pthread_t));
  pool->queue = (task_list *) calloc(1, sizeof(task_list));

  if (0 > (pool->l_socket = socket(AF_UNIX, SOCK_DGRAM, 0)))
    return -1;

  memset(&pool->main_t_address, 0, sizeof(pool->main_t_address));
  pool->main_t_address.sun_family = AF_UNIX;
  strcpy(pool->main_t_address.sun_path, lsocket_name);

  if (pthread_mutex_init(&(pool->lock), NULL) ||
      pthread_cond_init(&(pool->notify), NULL) ||
      !pool->threads || !pool->queue) 
    goto error;

  for (i = 0; i < THREAD_NUM; i++)
  {
    if (pthread_create(&(pool->threads[i]), NULL, threadpool_thread, 
       (void*)pool))
      goto error;
    
    pool->thread_count++;
  }

  return 0;

error:
  threadpool_destroy(pool);
  return -1;
}

/*! \brief Adiciona uma tarefa ao pool de threads
 *
 * \param[in] function A funcao a ser executada
 * \param[in] Os argumentos para a funcao
 * \param[out] pool O pool de threads
 *
 * \return -1 Caso haja algum erro
 * \return 0 Caso OK
 */
int threadpool_add(void (*function)(void *), void *argument, 
                   threadpool *pool) 
{
  task_node *new_node = NULL;

  if (!pool || !function)
    return -1;

  if (pthread_mutex_lock(&(pool->lock)))
    return -1;

  if(!(new_node = alloc_task_node(function, argument)))
    return -1;
  append_task_to_list(new_node, pool->queue);

  if (pthread_cond_signal(&(pool->notify)))
    return -1;

  if (pthread_mutex_unlock(&pool->lock))
    return -1;

  return 0;
}

/*! \brief Para a execucao e elimina o pool de threads
 *
 * \param[out] pool O pool a ser eliminado
 *
 * \return -1 Caso haja erro
 * \return 0 Caso OK
 */
int threadpool_destroy(threadpool *pool)
{
  int i;

  if (!pool)
    return -1;

  if (pthread_mutex_lock(&(pool->lock)))
    return -1;

  pool->shut_down = 1;
  if (pthread_cond_broadcast(&(pool->notify)) ||
      pthread_mutex_unlock(&(pool->lock)))
    return -1;

  for (i = 0; i < pool->thread_count; i++)
    if (pthread_join(pool->threads[i], NULL))
        return -1;

  if (pool->threads)
    free(pool->threads);

  if (pool->queue)
    free(pool->queue);

  if (pthread_mutex_destroy(&(pool->lock)) ||
      pthread_cond_destroy(&(pool->notify)))
    return -1;

  return 0;
}

/*! \brief Aloca um no' para uma tarefa
 *
 * \param[in] function Ponteiro para a funcao
 * \param[in] argument Argumento para a funcao
 *
 * \return NULL Caso haja erro
 * \return task_node Caso ok
 */
task_node *alloc_task_node(void (*function)(void *), void *argument)
{
  task_node *new_node = NULL;
  
  if(!(new_node = (task_node *) calloc(1, sizeof(task_node))))
    return NULL;

  new_node->function = function;
  new_node->argument = argument;

  return new_node;
}

/*! \brief Libera uma tarefa
 *
 * \param[out] node Tarefa a ser liberada
 *
 * \return -1 Caso haja erro
 * \return 0 Caso ok
 */
int free_task_node(task_node *node)
{
  if (!node)
    return -1;

  free(node);
  return 0;
}

/*! \brief Adiciona um elemento a lista
 *
 * \param[in] new_node O novo no a ser adicionado
 * \param[out] queue A lista
 */
void append_task_to_list(task_node *new_node, task_list *queue)
{
  task_node *last_node = NULL;

  if (!queue->size)
    queue->head = new_node;
  else
  {
    for(last_node = queue->head; last_node->next;
        last_node = last_node->next)
      ;

    last_node->next = new_node;
    new_node->before = last_node;
  }

  queue->size++;
}

/*! \brief Remove um elemento da lista
 *
 * \param[in] node O no a ser eliminado da lista
 * \param[out] queue A lista
 *
 * \return -1 Caso haja erro
 * \return 0 Caso ok
 */
int remove_task_f_list(task_node *node, task_list *queue)
{
  if (!node || !queue->size)
    return -1;

  if (node == queue->head)
    queue->head = node->next;
  else
  {
    if (node->next)
      node->next->before = node->before;

    if (node->before)
      node->before->next = node->next;
  }

  queue->size--;
  return 0;
}
