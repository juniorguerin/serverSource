/*!
 * \file token-bucket.c
 * \brief Codigo fonte para o token-bucket.h, que controla velocidade de
 * transmissao
 */

#include "token-bucket.h"

/*! \brief Atualiza a quantidade de tokens do bucket
 *
 * \param[out] bucket Estrutura do bucket do cliente
 */
static void bucket_update(Token_bucket *bucket)
{
  time_t now = 0;
  double elapsed_time = 0;
  
  time(&now);
  elapsed_time = difftime(now, bucket->last_fill);
  bucket->tokens += elapsed_time * bucket->rate;
  time(&bucket->last_fill);

  if (bucket->tokens >= bucket->burst)
  {
    bucket->tokens = bucket->burst;
    bucket->min_time = 0;
  }
  else
    bucket->min_time = (bucket->burst - bucket->tokens) / bucket->rate; 
}

/*! \brief Inicializa um bucket sem tokens
 *
 * \param[in] velocity Velocidade em kb/s
 * \param[in] burst Limite de tokens acumulados
 * \param[out] bucket Estrutura com informacoes do bucket
 */
void bucket_init(const unsigned int velocity, const unsigned int burst,
                       Token_bucket *bucket)
{
  bucket->rate = velocity * 1024;
  bucket->burst = burst;
  bucket->tokens = 0;
  time(&bucket->last_fill);
}

/*! \brief Muda e reseta as configuracoes de um determinado bucket
 *
 * \param[in] velocity Velocidade em kb/s
 * \param[in] burst Maximo de tokens
 * \param[out] bucket Estrutura com informacoes do bucket
 */
void bucket_set(const unsigned int velocity, const unsigned int burst,
                      Token_bucket *bucket)
{
  bucket->rate = velocity * 1024;
  bucket->burst = burst;
  
  if (bucket->burst > bucket->tokens)
    bucket->tokens = bucket->burst;

  time(&bucket->last_fill);
}

/*! \brief Remove uma quantidade especifica de tokens do bucket
 *
 * \param[in] remove_tokens Quantidade de tokens a ser removida
 * \param[ou] bucket Estrutura com informacoes do bucket
 */
int bucket_withdraw(const unsigned int remove_tokens,
                          Token_bucket *bucket)
{
  token_bucket_update(bucket);

  if (bucket->tokens < remove_tokens)
    return -1;

  bucket->tokens -= remove_tokens;
  return 0;
}

/*! \brief Faz atualizacao de todos os buckets e retorna o tempo minimo
 * para que algum cliente possa realizar transmissao
 *
 * \param[out] clients O vetor de clientes
 *
 * \return 0 Caso algum cliente possa realizar transmissao
 * \return Outro valor positivo caso nao haja cliente capaz de fazer uma
 * transmissao
 */
int buckets_get_time(Client *clients)
{
  int i = 0;
  float min_time = UINT_MAX;

  for (i = 0; i < FD_SETSIZE; i++)
  {
    token_bucket_update(clients[i].bucket);
    if (clients[i].min_time < min_time)
      min_time = clients[i].bucket.min_time;
  }

  return min_time;
}

