/*!
 * \file token-bucket.h
 * \brief Header com funcoes de controle de velocidade de transmissao
 */

#ifndef TOKEN_BUCKET_H
#define TOKEN_BUCKET_H

#include <time.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct token_bucket_
{
  unsigned int rate; /*!< Tokens adicionados em milisegundos */
  unsigned int tokens; /*!< Numero atual de tokens */
  unsigned int transmission:1; /*!< Flag para transmissao */
} token_bucket;

void bucket_init(const unsigned int velocity, token_bucket *bucket);

int bucket_withdraw(const unsigned int remove_tokens,
                          token_bucket *bucket);

void bucket_fill(token_bucket *bucket);

int bucket_token_status(token_bucket *bucket, const unsigned int tokens);

long timeval_subtract(struct timeval *cur_time, 
                      struct timeval *last_time);

void sleep_diff_burst(struct timeval *cur_time, 
                      struct timeval *last_burst, long period);

#endif
