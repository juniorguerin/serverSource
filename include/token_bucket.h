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
  int remain_tokens; /*!< Numero atual de tokens */
  int transmission; /*!< Flag para transmissao */
} token_bucket;

void bucket_init(const int rate, token_bucket *bucket);

int bucket_withdraw(const int remove_tokens, token_bucket *bucket);

void bucket_fill(const int rate, token_bucket *bucket);

void bucket_burst_remain_time(const struct timespec *burst_cur_time,
                              struct timespec *burst_rem_time);

int bucket_burst_init(struct timespec *burst_ini_time,
                      struct timespec *burst_cur_time);

void timespecsub(const struct timespec *cur_time,
                 const struct timespec *last_time,
                 struct timespec *result);

int timespecisset(struct timespec *time);

#endif
