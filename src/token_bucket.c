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
 * \return timespec O tempo restante
 *
 */
void bucket_burst_remain_time(const struct timespec *burst_cur_time,
                              struct timespec *burst_rem_time)
{
  struct timespec burst_total_time;

  memset(burst_rem_time, 0, sizeof(*burst_rem_time));
  memset(&burst_total_time, 0, sizeof(burst_total_time));
  burst_total_time.tv_sec = BURST_TIME;

  timespecsub(&burst_total_time, burst_cur_time, burst_rem_time);
}

/*! \brief Recarrega todos os buckets a cada 1 segundo
 *
 * \param[in] burst_ini_time O momento da ultima recarga de tokens
 * \param[out] burst_cur_time Tempo da burst atual
 *
 * \return -1 Caso haja erro no relogio
 * \return 0 Caso ok
 */
int bucket_burst_init(struct timespec *burst_ini_time,
                       struct timespec *burst_cur_time)
{
  struct timespec cur_time;

  if (0 > clock_gettime(CLOCK_MONOTONIC, &cur_time))
    return -1;

  timespecsub(&cur_time, burst_ini_time, burst_cur_time);

  if (burst_cur_time->tv_sec >= 1)
  {
    *burst_ini_time = cur_time;
    memset(burst_cur_time, 0, sizeof(*burst_cur_time));
  }

  return 0;
}

/* \brief Realiza a subtracao de dois struct timespec
 *
 * \param[in] cur_time O tempo mais recente
 * \param[in] last_time O tempo anterior
 * \param[out] result O resultado
 *
 * \note Caso o tempo anterior seja maior que o atual, retorna a estrutura nula
 */
void timespecsub(const struct timespec *cur_time,
                 const struct timespec *last_time,
                 struct timespec *result)
{
  time_t dif_sec = cur_time->tv_sec - last_time->tv_sec;
  long dif_nsec = cur_time->tv_nsec - last_time->tv_nsec;
  long long total_dif = 1000000000*dif_sec + dif_nsec;
  memset(result, 0, sizeof(*result));

  if (0 < total_dif)
  {
    if (0 > dif_nsec)
    {
      result->tv_nsec = 1000000000 + dif_nsec;
      result->tv_sec = dif_sec - 1;
    }
    else
    {
      result->tv_nsec = dif_nsec;
      result->tv_sec = dif_sec;
    }
  }
}

/* \brief Verifica se um timespec tem valores
 *
 * \param[in] time O tempo a ser verificado
 *
 * \return 1 Caso tenha valores
 * \return 0 Caso contrario
 */
int timespecisset(struct timespec *time)
{
  if (!time->tv_sec && !time->tv_nsec)
    return 0;

  return 1;
}
