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
  bucket->tokens = bucket->rate;
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
  if (0 > bucket_token_status(bucket, remove_tokens))
    return -1;

  bucket->tokens -= remove_tokens;
  return 0;
}

/*! \brief Enche o bucket e coloca a flag de tramissao como 1
 *
 * \param[in] bucket O bucket em questao
 */
void bucket_fill(token_bucket *bucket)
{
  bucket->tokens = bucket->rate;
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
int bucket_token_status(token_bucket *bucket, unsigned int tokens)
{
  if (bucket->tokens < tokens)
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
 * \return -1 Caso o tempo anterior seja maior que o recente
 */
long timeval_subtract(struct timeval *cur_time, 
                     struct timeval *last_time)
{
  int usec = cur_time->tv_usec - last_time->tv_usec;
  int sec = (cur_time->tv_sec - last_time->tv_sec);
 
  if (0 > sec)
    return -1;

  return sec*1000000 + usec;
}

/*! \brief Coloca a thread em sleep pela diferenca de tempo do periodo da ultima
 * burst
 *
 * \param[in] cur_time Tempo atual
 * \param[in] last_burst Tempo da ultima burst
 * \param[in] period O periodo de tempo entre cada burst em segundos
 */
void sleep_diff_burst(struct timeval *cur_time, 
                      struct timeval *last_burst, long period)
{
  usleep(period*1000000 - timeval_subtract(cur_time, last_burst));
}
