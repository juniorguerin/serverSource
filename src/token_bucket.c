/*!
 * \file token-bucket.c
 * \brief Codigo fonte para o token-bucket.h, que controla velocidade de
 * transmissao
 */

#include "token_bucket.h"

/*! \brief Inicializa um bucket com limite de tokens
 *
 * \param[in] velocity Velocidade em kb/s
 * \param[out] bucket Bucket em questao
 */
void bucket_init(const int velocity, token_bucket *bucket)
{
  bucket->rate = velocity;
  bucket->remain_tokens = bucket->rate;
  bucket->transmission = 1;
}

/*! \brief Remove uma quantidade especifica de tokens do bucket 
 *
 * \param[in] remove_tokens Quantidade de tokens a ser removida
 * \param[ou] bucket Bucket em questao
 *
 * \return 0 Caso ok
 * \return -1 Caso nao tenha os tokens
 */
int bucket_withdraw(const int remove_tokens,
                          token_bucket *bucket)
{
  bucket->remain_tokens -= remove_tokens;
  if(0 >= bucket->remain_tokens)
  {
    bucket->transmission = 0;
    bucket->remain_tokens = 0;
    return -1;
  }

  return 0;
}

/*! \brief Enche o bucket e coloca a flag de tramissao como 1
 *
 * \param[in] bucket O bucket em questao
 */
void bucket_fill(token_bucket *bucket)
{
  bucket->remain_tokens = bucket->rate;
  bucket->transmission = 1;
}

/*! \brief Calcula a diferenca de tempo em microsegundos
 *
 * \param[in] cur_time Momento mais recente
 * \param[in] last_time Momento anterior
 *
 * \return timeval A diferenca de tempo
 */
struct timeval timeval_subtract(const struct timeval *cur_time, 
                                const struct timeval *last_time)
{
  struct timeval timeval_diff;
  
  memset(&timeval_diff, 0, sizeof(timeval_diff));

  long usec = cur_time->tv_usec - last_time->tv_usec;
  long sec = cur_time->tv_sec - last_time->tv_sec;

  if (0 > sec)
    return timeval_diff;
  
  if (0 > usec)
  {
    sec -= 1;
    usec = 1000000 + usec;
  }

  timeval_diff.tv_usec = usec;
  timeval_diff.tv_sec = sec;
  return timeval_diff; 
}

/*! \brief Retorna tempo restante para o fim da burst atual
 *
 * \param[in] burst_cur_time O tempo atual da burst
 * \param[out] burst_remain_time O tempo restante para a burst
 *
 * \return timeval O tempo restante
 *
 */
struct timeval burst_remain_time(const struct timeval *burst_cur_time)
{
  struct timeval burst_rem_time;
  memset(&burst_rem_time, 0, sizeof(burst_rem_time));

  if (burst_cur_time->tv_sec < BURST_TIME)
    burst_rem_time.tv_usec = labs(BURST_U_TIME - 
                                   burst_cur_time->tv_usec);

  return burst_rem_time;
}
