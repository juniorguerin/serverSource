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
  threadpool *pool = (threadpool *) cur_threadpool;
  task_node task;

  while (1)
  {
    pthread_mutex_lock(&(pool->lock));

    while (!pool->queue->size)
      pthread_cond_wait(&(pool->notify), &(pool->lock));

    task.function = pool->queue->head->function;
    task.argument = pool->queue->head->argument;
    remove_task_f_list(pool->queue->head, pool->queue);
    free_task_node(pool->queue->head);

    pthread_mutex_unlock(&(pool->lock));

    (*(task.function))(task.argument);
    
    // quando a funcao terminar, comunica com um socket a thread 
    // principal (analisar)
  }

  pthread_mutex_unlock(&(pool->lock));
}

/*! \brief Funcao que cria um pool de threads
 *
 * \param[in] thread_count O numero de threads
 *
 * \return NULL Caso haja erro
 * \return threadpool O pool de threads
 */
threadpool *threadpool_create(int thread_count)
{
  threadpool *pool;
  int i;

  if (!(pool = (threadpool *) calloc(1, sizeof(threadpool)))) 
    goto error;

  pool->threads = (pthread_t *) calloc(thread_count, 
                                       sizeof(pthread_t));
  pool->queue = (task_list *) calloc (1, sizeof(task_list));

  if (pthread_mutex_init(&(pool->lock), NULL) ||
      pthread_cond_init(&(pool->notify), NULL) ||
      !pool->threads || !pool->queue) 
    goto error;

  for (i = 0; i < thread_count; i++)
  {
    if(pthread_create(&(pool->threads[i]), NULL, threadpool_thread, 
       (void*)pool))
    {
      threadpool_destroy(pool);
      return NULL;
    }
    
    pool->thread_count++;
  }

  return pool;

error:
  if (pool)
    threadpool_free(pool);
  
  return NULL;
}

/*! \brief Adiciona uma tarefa ao pool de threads
 *
 * \param[in] (*function) A tarefa / funcao  a ser realizada
 * \param[in] Os argumentos para a funcao
 * \param[out] pool O pool de threads
 *
 * \return -1 Caso haja algum erro
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

/*! \brief Procedimentos iniciais para destruir o pool de threads
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

  if (pthread_cond_broadcast(&(pool->notify)) ||
      pthread_mutex_unlock(&(pool->lock)))
    return -1;

  for (i = 0; i < pool->thread_count; i++)
    if (pthread_join(pool->threads[i], NULL))
        return -1;

  threadpool_free(pool);
  return 0;
}

/*! \brief Libera as variaveis do pool de threads
 *
 * \param[out] pool O pool de threads
 *
 * \return -1 Caso haja algum problema
 * \return 0 Caso ok
 */
int threadpool_free(threadpool *pool)
{
  if (!pool)
    return -1;

  if (pool->threads)
  {
    free(pool->threads);
    free(pool->queue);
    pthread_mutex_lock(&(pool->lock));
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
  }

  free(pool);    
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

  if (queue->size == 1)
    queue->head = node->next;

  if (node->next)
    node->next->before = node->before;

  if (node->before)
    node->before->next = node->next;
  
  return 0;
}

    