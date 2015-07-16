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

/*! \brief Retorna tempo restante para o fim da burst atual
 *
 * \param[in] burst_cur_time O tempo atual da burst
 * \param[out] burst_remain_time O tempo restante para a burst
 *
 * \return timeval O tempo restante
 *
 */
void bucket_burst_remain_time(const struct timeval *burst_cur_time, 
                              struct timeval *burst_rem_time)
{
  struct timeval burst_total_time;

  timerclear(burst_rem_time);
  burst_total_time.tv_sec = BURST_TIME;

  timersub(&burst_total_time, burst_cur_time, burst_rem_time);
}

/*! \brief Recarrega todos os buckets a cada 1 segundo
 *
 * \param[in] burst_ini_time O momento da ultima recarga de tokens
 * \param[out] burst_cur_time Tempo da burst atual
 */
void bucket_burst_init(struct timeval *burst_ini_time, 
                       struct timeval *burst_cur_time)
{
  struct timeval cur_time;

  gettimeofday(&cur_time, NULL);
  timersub(&cur_time, burst_ini_time, burst_cur_time);

  if (burst_cur_time->tv_sec >= 1)
  {
    *burst_ini_time = cur_time;
    timerclear(burst_cur_time);
  }
}
