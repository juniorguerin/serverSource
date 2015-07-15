/*!
 * \file token-bucket.h
 * \brief Header com funcoes de controle de velocidade de transmissao
 */

#ifndef TOKEN_BUCKET_H
#define TOKEN_BUCKET_H

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define BURST_U_TIME 1000000
#define BURST_TIME 1

typedef struct token_bucket_
{
  int rate; /*!< Tokens adicionados em milisegundos */
  int remain_tokens; /*!< Numero atual de tokens */
  int transmission:1; /*!< Flag para transmissao */
} token_bucket;

void bucket_init(const int velocity, token_bucket *bucket);

int bucket_withdraw(const int remove_tokens, token_bucket *bucket);

void bucket_fill(token_bucket *bucket);

struct timeval timeval_subtract(const struct timeval *cur_time, 
                                const struct timeval *last_time);

struct timeval burst_remain_time(const struct timeval *burst_cur_time);

#endif
