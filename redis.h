/*
 * redis.h
 *
 *  Created on: Apr 21, 2010
 *      Author: Sami Bouafif
 */

#ifndef REDIS_H_
#define REDIS_H_
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef struct _REDIS
{
  int   fd;                     /* socket descriptor to Redis Server      */
  char  host[INET6_ADDRSTRLEN]; /* Redis server host                      */
  char  *port;                  /* Redis server port, service name or num */
  int   lasterror;              /* Last error                             */
  char  *errorstr;              /* Error details                          */
} REDIS;

typedef enum
{
  REDIS_RETURN_ERROR,
  REDIS_RETURN_LINE,
  REDIS_RETURN_BULK,
  REDIS_RETURN_MULTIBULK,
  REDIS_RETURN_INTEGER
} redis_returnType;

typedef enum
{
  REDIS_EXEC_DEFAULT,
  REDIS_EXEC_MULTIBULK
} redis_execType;

typedef struct
{
  redis_returnType type;
  int              has_error;
  char             *errorMsg;
  char             *line;
  char             *bulk;
  char             **multibulk;
  int              multibulkSize;
  int              integer;
} redis_returnValue;


REDIS* redis_connect(char *host, char *port);
int redis_send(REDIS *redis, char *data, size_t size);
char* redis_receive(REDIS *redis);
void redis_close(REDIS *redis);

char* redis_buildCommand(int argsc, char **argsv);

char *redis_genMultiBulk (int argsc, char **argsv);
char *redis_genBulk (int argsc, char **argsv);
char *redis_genInline (int argsc, char **argsv);

redis_returnValue *redis_parseErrorReturn(char* rdata);
redis_returnValue *redis_parseBulkReturn(char* rdata);
redis_returnValue *redis_parseMultiBulkReturn(char* rdata);
void redis_freeResult(redis_returnValue *rv);
redis_returnValue *redis_parseReturn(char *rdata);

redis_returnValue *redis_exec(REDIS *redis, redis_execType execType, ...);
#endif /* REDIS_H_ */
