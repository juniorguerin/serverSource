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
void bucket_init(const unsigned int velocity, token_bucket *bucket)
{
  bucket->rate = velocity * 1024;
  bucket->remain_tokens = bucket->rate;
  bucket->transmission = 1;
}

/*! \brief Remove uma quantidade especifica de tokens do bucket 
 *
 * \param[in] remove_tokens Quantidade de tokens a ser removida
 * \param[ou] bucket Bucket em questao
 *
 * \return 0 Caso ok
 * \return -1 Caso nao tenha a quantidade de tokens
 */
int bucket_withdraw(const unsigned int remove_tokens,
                          token_bucket *bucket)
{
  if (0 > bucket_verify_tokens(bucket, remove_tokens))
    return -1;

  bucket->remain_tokens -= remove_tokens;
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

/*! \brief Verifica se o bucket tem determinada quantidade de tokens
 *
 * \param[in] bucket O bucket a ser verificado
 * \param[in] tokens A quantidade de tokens
 *
 * \return 0 Caso OK
 * \return -1 Caso nao tenha a quantidade de buckets
 *
 * \note Coloca a flag de transmissao como 0 caso nao tenha tokens
 */
int bucket_verify_tokens(token_bucket *bucket, unsigned int tokens)
{
  if (bucket->remain_tokens < tokens)
  {
    bucket->transmission = 0;
    return -1;
  }

  return 0;
}

/*! \brief Calcula a diferenca de tempo em microsegundos
 *
 * \param[in] cur_time Momento mais recente
 * \param[in] last_time Momento anterior
 *
 * \return O tempo em microsegundos
 * \return 0 Caso o tempo anterior seja maior que o recente
 */
long timeval_subtract(struct timeval *cur_time, 
                               struct timeval *last_time)
{
  long usec = cur_time->tv_usec - last_time->tv_usec;
  long sec = cur_time->tv_sec - last_time->tv_sec;
  
  if (0 > sec)
    return 0;

  return sec*1000000 + usec; 
}

/*! \brief Coloca a thread em sleep pelo restante do tempo de 1s com relação a
 * ultima burst
 *
 * \param[in] cur_time Tempo atual
 * \param[in] last_burst Tempo da ultima burst
 */
void sleep_burst_diff(struct timeval *cur_time, 
                      struct timeval *last_burst)
{
  long diff_time = timeval_subtract(cur_time,last_burst);
  unsigned long time_to_sleep = labs(1000000 - diff_time);
  usleep(time_to_sleep);
}
