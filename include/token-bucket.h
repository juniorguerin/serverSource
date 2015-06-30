/*!
 * \file token-bucket.h
 * \brief Header com funcoes de controle de velocidade de transmissao
 */

#ifndef TOKEN_BUCKET_H
#define TOKEN_BUCKET_H

#include<time.h>
#include<limits.h>

typedef struct Token_bucket_
{
  unsigned int rate; /*!< Tokens adicionados em milisegundos */
  unsigned int burst; /*!< Maximo de creditos em tokens */
  unsigned int tokens; /*!< Numero atual de tokens */
  clock_t last_fill; /*!< Ultima adicao */
  unsigned int min_time; /*!< Tempo minimo para receber buffer */
} Token_bucket;

void token_bucket_init(const unsigned int rate, const unsigned int burst,
                       Token_bucket *bucket);

void token_bucket_set(const unsigned int rate, const unsigned int burst,
                      Token_bucket *bucket);

int token_bucket_withdraw(const unsigned int remove_tokens,
                          Token_bucket *bucket);

#endif
