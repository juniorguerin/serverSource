/*!
 * \file multithread.h
 * \brief Interface com funcoes para pool de threads
 */

#ifndef MULTITHREAD_H
#define MULTITHREAD_H

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct task_node_ {
    void (*function)(void *); /*<! Ponteiro para a funcao da tarefa */
    void *argument; /*<! Argumento passado para a funcao */
    struct task_node_ *next; /*<! Proximo elemento da lista */
    struct task_node_ *before; /*<! Elemento anterior */
} task_node;

task_node *alloc_task_node(void (*function)(void *), void *argument);

int free_task_node(task_node *node);

typedef struct task_list_ {
  task_node *head; /*<! Cabeca da lista */
  int size; /*<! Tamanho da lista */
} task_list;

void append_task_to_list(task_node *new_node, task_list *queue);

int remove_task_f_list(task_node *node, task_list *queue);

typedef struct threadpool_ {
  pthread_mutex_t lock; /*<! Variavel para mutex */
  pthread_cond_t notify; /*<! Variavel para notificar threads */
  pthread_t *threads; /*<! Array de threads */
  task_list *queue; /*<! Array de queue */
  int thread_count; /*<! Numero de threads */
  int queue_size; /*<! Tamanho da queue */
} threadpool;

int threadpool_free(threadpool *pool);

threadpool *threadpool_create(int thread_count);

int threadpool_add(void (*function)(void *), void *argument, 
                   threadpool *pool);

int threadpool_destroy(threadpool *pool);

#endif