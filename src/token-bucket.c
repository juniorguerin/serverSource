/*!
 * \file token-bucket.c
 * \brief Codigo fonte para o token-bucket.h, que controla velocidade de
 * transmissao
 */

#include "token-bucket.h"

/*! \brief Inicializa um bucket sem tokens
 *
 * \param[in] rate 
 * \param[in] burst
 * \param[out] bucket Estrutura com informacoes do bucket
 */
void token_bucket_init(const unsigned int rate, const unsigned int burst,
                       Token_bucket *bucket)
{
  bucket->rate = rate;
  bucket->burst = burst;
  bucket->tokens = 0;
  bucket->last_fill = clock();
}

/*! \brief Muda e reseta as configuracoes de um determinado bucket
 *
 * \param[in] rate Quantidade de tokens por milisegundo
 * \param[in] burst Maximo de tokens
 * \param[out] bucket Estrutura com informacoes do bucket
 */
void token_bucket_set(const unsigned int rate, const unsigned int burst,
                      Token_bucket *bucket)
{
  bucket->rate = rate; // chega em kb/s e faz conversao para b/ms
  bucket->burst = burst;
  
  if (bucket->burst > bucket->tokens)
    bucket->tokens = bucket->burst;

  bucket->last_fill = clock();
}

/* FAZER */
static void token_bucket_add(const float elapsed_time, Token_bucket *bucket)
{
  unsigned int new_tokens = 0;

  new_tokens = (elapsed_time / 1000.0F) * bucket->rate;
  bucket->tokens += new_tokens;

  if (bucket->tokens > bucket->burst)
    bucket->tokens = bucket->burst;

  bucket->last_fill = clock();
}

/* FAZER */
int token_bucket_withdraw(const unsigned int remove_tokens,
                          Token_bucket *bucket)
{
  if (bucket->tokens < remove_tokens)
  {
    clock_t now = clock();
    if (now > bucket->last_fill)
    {
      float elapsed_time = now - bucket->last_fill;
      token_bucket_add(elapsed_time, bucket);
    }

    if (bucket->tokens < remove_tokens)
      return -1;
  }

  bucket->tokens -= remove_tokens;
  return 0;
}

