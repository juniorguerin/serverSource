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

#define BURST_TIME 1

typedef struct token_bucket_
{
  int rate; /*!< Taxa de adicao de tokens (bytes) por segundo */
  int remain_tokens; /*!< Numero atual de tokens */
  int transmission; /*!< Flag para transmissao */
} token_bucket;

void bucket_init(const int velocity, token_bucket *bucket);

int bucket_withdraw(const int remove_tokens, token_bucket *bucket);

void bucket_fill(token_bucket *bucket);

void bucket_burst_remain_time(const struct timeval *burst_cur_time, 
                              struct timeval *burst_rem_time);

void bucket_burst_init(struct timeval *last_fill, 
                       struct timeval *burst_cur_time);
#endif
