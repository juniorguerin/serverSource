/*!
 * \file token-bucket.c
 * \brief Codigo fonte para o token-bucket.h, que controla velocidade de
 * transmissao
 */

#include "token-bucket.h"

/*! \brief Inicializa um bucket com limite de tokens
 *
 * \param[in] velocity Velocidade em kb/s
 * \param[out] bucket Bucket em questao
 */
void bucket_init(const unsigned int velocity, Token_bucket *bucket)
{
  bucket->fill_rate = velocity * 1024;
  bucket->tokens = bucket_fill_rate;
  bucket->transmission = 1;
}

/*! \brief Remove uma quantidade especifica de tokens do bucket e coloca a 
 * flag de tranmissao como 0 se nao for possivel receber o proximo buffer
 *
 * \param[in] remove_tokens Quantidade de tokens a ser removida
 * \param[ou] bucket Bucket em questao
 */
int bucket_withdraw(const unsigned int remove_tokens,
                          Token_bucket *bucket)
{
  if (bucket->tokens < remove_tokens)
  {
    transmission = 0;
    return -1;
  }

  bucket->tokens -= remove_tokens;

  if (bucket->tokens < BUFFER_LEN)
    transmission = 0;

  return 0;
}

/*! \brief Enche o bucket e coloca a flag de tramissao como 1
 *
 * \param[in] bucket O bucket em questao
 */
void bucket_fill(token_bucket *bucket)
{
  bucket->tokens = fill_rate;
  bucket->transmission = 1;
}
