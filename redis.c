/*
 * redis.c
 *
 *  Created on: Apr 21, 2010
 *      Author: Sami Bouafif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <redis.h>
/* Basic structure */

#define MAXDATASIZE  1024
#define MAXSTRLENGTH 1024

/* Taken +/- verbatim from rediscli */
enum
{
  REDIS_CMD_INLINE,
  REDIS_CMD_BULK,
  REDIS_CMD_MULTIBULK
};

struct redisCommand
{
  char *name;
  int  arity;
  int  flags;
};

static struct redisCommand cmdTable[] = {
    {"auth",2,REDIS_CMD_INLINE},
    {"get",2,REDIS_CMD_INLINE},
    {"set",3,REDIS_CMD_BULK},
    {"setnx",3,REDIS_CMD_BULK},
    {"append",3,REDIS_CMD_BULK},
    {"substr",4,REDIS_CMD_INLINE},
    {"del",-2,REDIS_CMD_INLINE},
    {"exists",2,REDIS_CMD_INLINE},
    {"incr",2,REDIS_CMD_INLINE},
    {"decr",2,REDIS_CMD_INLINE},
    {"rpush",3,REDIS_CMD_BULK},
    {"lpush",3,REDIS_CMD_BULK},
    {"rpop",2,REDIS_CMD_INLINE},
    {"lpop",2,REDIS_CMD_INLINE},
    {"brpop",-3,REDIS_CMD_INLINE},
    {"blpop",-3,REDIS_CMD_INLINE},
    {"llen",2,REDIS_CMD_INLINE},
    {"lindex",3,REDIS_CMD_INLINE},
    {"lset",4,REDIS_CMD_BULK},
    {"lrange",4,REDIS_CMD_INLINE},
    {"ltrim",4,REDIS_CMD_INLINE},
    {"lrem",4,REDIS_CMD_BULK},
    {"rpoplpush",3,REDIS_CMD_BULK},
    {"sadd",3,REDIS_CMD_BULK},
    {"srem",3,REDIS_CMD_BULK},
    {"smove",4,REDIS_CMD_BULK},
    {"sismember",3,REDIS_CMD_BULK},
    {"scard",2,REDIS_CMD_INLINE},
    {"spop",2,REDIS_CMD_INLINE},
    {"srandmember",2,REDIS_CMD_INLINE},
    {"sinter",-2,REDIS_CMD_INLINE},
    {"sinterstore",-3,REDIS_CMD_INLINE},
    {"sunion",-2,REDIS_CMD_INLINE},
    {"sunionstore",-3,REDIS_CMD_INLINE},
    {"sdiff",-2,REDIS_CMD_INLINE},
    {"sdiffstore",-3,REDIS_CMD_INLINE},
    {"smembers",2,REDIS_CMD_INLINE},
    {"zadd",4,REDIS_CMD_BULK},
    {"zincrby",4,REDIS_CMD_BULK},
    {"zrem",3,REDIS_CMD_BULK},
    {"zremrangebyscore",4,REDIS_CMD_INLINE},
    {"zmerge",-3,REDIS_CMD_INLINE},
    {"zmergeweighed",-4,REDIS_CMD_INLINE},
    {"zrange",-4,REDIS_CMD_INLINE},
    {"zrank",3,REDIS_CMD_BULK},
    {"zrevrank",3,REDIS_CMD_BULK},
    {"zrangebyscore",-4,REDIS_CMD_INLINE},
    {"zcount",4,REDIS_CMD_INLINE},
    {"zrevrange",-4,REDIS_CMD_INLINE},
    {"zcard",2,REDIS_CMD_INLINE},
    {"zscore",3,REDIS_CMD_BULK},
    {"incrby",3,REDIS_CMD_INLINE},
    {"decrby",3,REDIS_CMD_INLINE},
    {"getset",3,REDIS_CMD_BULK},
    {"randomkey",1,REDIS_CMD_INLINE},
    {"select",2,REDIS_CMD_INLINE},
    {"move",3,REDIS_CMD_INLINE},
    {"rename",3,REDIS_CMD_INLINE},
    {"renamenx",3,REDIS_CMD_INLINE},
    {"keys",2,REDIS_CMD_INLINE},
    {"dbsize",1,REDIS_CMD_INLINE},
    {"ping",1,REDIS_CMD_INLINE},
    {"echo",2,REDIS_CMD_BULK},
    {"save",1,REDIS_CMD_INLINE},
    {"bgsave",1,REDIS_CMD_INLINE},
    {"rewriteaof",1,REDIS_CMD_INLINE},
    {"bgrewriteaof",1,REDIS_CMD_INLINE},
    {"shutdown",1,REDIS_CMD_INLINE},
    {"lastsave",1,REDIS_CMD_INLINE},
    {"type",2,REDIS_CMD_INLINE},
    {"flushdb",1,REDIS_CMD_INLINE},
    {"flushall",1,REDIS_CMD_INLINE},
    {"sort",-2,REDIS_CMD_INLINE},
    {"info",1,REDIS_CMD_INLINE},
    {"mget",-2,REDIS_CMD_INLINE},
    {"expire",3,REDIS_CMD_INLINE},
    {"expireat",3,REDIS_CMD_INLINE},
    {"ttl",2,REDIS_CMD_INLINE},
    {"slaveof",3,REDIS_CMD_INLINE},
    {"debug",-2,REDIS_CMD_INLINE},
    {"mset",-3,REDIS_CMD_MULTIBULK},
    {"msetnx",-3,REDIS_CMD_MULTIBULK},
    {"monitor",1,REDIS_CMD_INLINE},
    {"multi",1,REDIS_CMD_INLINE},
    {"exec",1,REDIS_CMD_INLINE},
    {"discard",1,REDIS_CMD_INLINE},
    {"hset",4,REDIS_CMD_MULTIBULK},
    {"hget",3,REDIS_CMD_BULK},
    {"hdel",3,REDIS_CMD_BULK},
    {"hlen",2,REDIS_CMD_INLINE},
    {"hkeys",2,REDIS_CMD_INLINE},
    {"hvals",2,REDIS_CMD_INLINE},
    {"hgetall",2,REDIS_CMD_INLINE},
    {"hexists",3,REDIS_CMD_BULK},
    {"config",-2,REDIS_CMD_BULK},
    {NULL,0,0}
};

static struct redisCommand *lookup_command(char *name)
{
  int j = 0;
  while(cmdTable[j].name != NULL)
  {
    if (!strcasecmp(name,cmdTable[j].name)) return &cmdTable[j];
    j++;
  }
  return NULL;
}

/* utilities functions */
static int redis_ascatprintf(char** s, const char *fmt, ...)
{
  va_list ap;
  char    *buf;
  int     rc;

  va_start(ap, fmt);
  rc = vasprintf(&buf, fmt, ap);
  if (rc < 0) return -1;
  va_end(ap);

  if (*s == NULL)
  {
    *s = buf;
    return 0;
  }

  *s = realloc(*s, (strlen(*s) + strlen(buf)+1) * sizeof(char));
  if (*s == NULL) return -1;
  *s = strcat(*s, buf);
  if (*s == NULL) return -1;

  free(buf);
  return 0;
}

static void redis_free(REDIS *redis)
{
  if (redis == NULL) return;
  close(redis->fd);
  if (redis->port) free(redis->port);
  /*if (redis->errorstr) free(redis->errorstr);*/
  free(redis);
}

/* Connect to host */
REDIS* redis_connect(char *host, char *port)
{
  struct addrinfo hints;
  struct addrinfo *servinfo;
  struct addrinfo *p;
  int    optval = 1;
  REDIS  *redis;
  char   *servername;
  char   *serverport;
  void   *addr;

  redis = (REDIS*) malloc(sizeof(REDIS));
  if (redis == NULL) return NULL;

  redis->port = NULL;
  servername = host ? host
                    : "127.0.0.1";
  serverport = port ? port
                    : "6379";
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(servername, serverport, &hints, &servinfo) != 0)
  {
    redis_free(redis);
    return NULL;
  }

  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    redis->fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    /* Set socket options */
    if (setsockopt(redis->fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval) == -1||
        setsockopt(redis->fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval) == -1)
    {
      freeaddrinfo(servinfo);
      redis_free(redis);
      return NULL;
    }

    if (redis->fd == -1) continue;
    if (connect(redis->fd, p->ai_addr, p->ai_addrlen) != -1) break;
  }

  if (p == NULL)
  {
    freeaddrinfo(servinfo);
    redis_free(redis);
    return NULL;
  }
  /* allocate storage */
  if (p->ai_family == AF_INET)
  {
    struct sockaddr_in *ipv4;
    ipv4 = (struct sockaddr_in *)p->ai_addr;
    addr = &(ipv4->sin_addr);
  }
  else
  {
    struct sockaddr_in6 *ipv6;
    ipv6 = (struct sockaddr_in6 *)p->ai_addr;
    addr = &(ipv6->sin6_addr);
  }

  redis->port = malloc(sizeof(serverport));

  if (redis->host == NULL || redis->port == NULL)
  {
    freeaddrinfo(servinfo);
    redis_free(redis);
    return NULL;
  }
  inet_ntop(p->ai_family, addr, redis->host, sizeof(redis->host));
  strcpy(redis->port, serverport);
  freeaddrinfo(servinfo);
  return redis;
}


int redis_send(REDIS *redis, char *data, size_t size)
{
  size_t sent;
  size_t n;
  fd_set fds;
  struct timeval tv;
  int rc;

  tv.tv_sec = 10;
  tv.tv_usec = 0;
  sent = 0;

  while (sent < size)
  {
    FD_ZERO(&fds);
    FD_SET(redis->fd, &fds);
    rc = select(redis->fd+1, NULL, &fds, NULL, &tv);
    if (rc <= 0) break;
    n = send(redis->fd, data+sent, size-sent, 0);
    if (n == -1) return -1;
    sent += n;
  }
  if (rc == 0)  return sent;
  if (rc == -1) return -1;
  return 0;
}

char* redis_receive(REDIS *redis)
{
  char buffer[MAXDATASIZE];
  char *data = NULL;
  int n;
  fd_set fds;
  struct timeval tv;
  int rc;
  tv.tv_sec = 10;
  tv.tv_usec = 0;

  /*init data*/
  data = strdup("");
  if (data == NULL) return NULL;

  while (1)
  {
    int datalen = strlen(data);
    FD_ZERO(&fds);
    FD_SET(redis->fd, &fds);
    rc = select(redis->fd+1, &fds, NULL, NULL, &tv);
    if (rc <= 0) break;

    n = recv(redis->fd, buffer, MAXDATASIZE, 0);
    if (n == -1) return NULL;
    if (n ==  0) return data;
    data = (char *)realloc(data, sizeof(data) + n);
    /* data = realloc(data, sizeof(data) + n); */
    if (data == NULL) return NULL;
    data[datalen + n ] = '\0';
    memcpy(data + datalen, buffer, n);
    if (n < MAXDATASIZE) return data;
  }
  return NULL;
}

void redis_close(REDIS *redis)
{
  redis_free(redis);
}

char* redis_buildCommand(int argsc, char **argsv)
{
  struct redisCommand *redis_cmd;

  redis_cmd = lookup_command(argsv[0]);
  if (!redis_cmd) return NULL;
  if ((redis_cmd->arity > 0 && argsc != redis_cmd->arity) ||
      (redis_cmd->arity < 0 && argsc < -redis_cmd->arity))
  {
    return NULL;
  }

  switch (redis_cmd->flags)
  {
    case REDIS_CMD_MULTIBULK : return redis_genMultiBulk(argsc, argsv);
    case REDIS_CMD_BULK      : return redis_genBulk(argsc, argsv);
    case REDIS_CMD_INLINE    : return redis_genInline(argsc, argsv);
  }
  return NULL;
}

char *redis_genMultiBulk (int argsc, char **argsv)
{
  char *cmd;
  int  rc, i;

  rc = asprintf(&cmd, "*%d\r\n", argsc);
  if (rc < 0) return NULL;
  for (i=0; i < argsc; i++)
  {
    rc = redis_ascatprintf(&cmd, "$%lu\r\n%s\r\n", strlen(argsv[i]), argsv[i]);
    if (rc < 0) return NULL;
  }
  return cmd;
}

char *redis_genBulk (int argsc, char **argsv)
{
  char *cmd = NULL;
  int  rc, i;

  for (i=0; i < argsc-1; i++)
  {
    rc = redis_ascatprintf(&cmd, "%s ", argsv[i]);
    if (rc < 0) return NULL;
  }
  rc = redis_ascatprintf(&cmd, "%lu\r\n%s\r\n", strlen(argsv[argsc-1]), argsv[argsc-1]);
  if (rc < -1) return NULL;
  return cmd;
}

char *redis_genInline (int argsc, char **argsv)
{
  char *cmd = NULL;
  int  rc, i;

  for (i=0; i < argsc; i++)
  {
    rc = redis_ascatprintf(&cmd, "%s ", argsv[i]);
    if (rc < 0) return NULL;
  }
  rc = redis_ascatprintf(&cmd, "\r\n");
  if (rc < -1) return NULL;
  return cmd;
}

redis_returnValue *redis_initReturnValue()
{
  redis_returnValue *rv;
  rv = (redis_returnValue *) malloc(sizeof(redis_returnValue));
  if (rv == NULL) return NULL;
  rv->bulk = NULL;
  rv->errorMsg = NULL;
  rv->multibulk = NULL;
  rv->line = NULL;
  return rv;
}

redis_returnValue *redis_parseErrorReturn(char* rdata)
{
  redis_returnValue *rv;
  size_t rdatalen;
  rv = redis_initReturnValue();
  if (rv == NULL) return NULL;
  rv->type = REDIS_RETURN_ERROR;
  rdatalen = strlen(rdata);
  rv->errorMsg = (char *)malloc((rdatalen-1) * sizeof(char));
  if (rv->errorMsg == NULL)
  {
    redis_freeResult(rv);
    return NULL;
  }
  rv->errorMsg[rdatalen-2] = '\0';
  rv->errorMsg = strncpy(rv->errorMsg, rdata, rdatalen-2);
  rv->has_error = 1;
  return rv;
}

redis_returnValue *redis_parseLineReturn(char* rdata)
{
  redis_returnValue *rv;
  size_t rdatalen;
  rv = redis_initReturnValue();
  if (rv == NULL) return NULL;
  rv->type = REDIS_RETURN_LINE;
  rdatalen = strlen(rdata);
  rv->line = (char *)malloc((rdatalen-1) * sizeof(char));
  if (rv->line == NULL)
  {
    redis_freeResult(rv);
    return NULL;
  }
  rv->line[rdatalen-2] = '\0';
  rv->line = strncpy(rv->line, rdata, rdatalen-2);
  rv->has_error = 0;
  return rv;
}

redis_returnValue *redis_parseIntegerReturn(char* rdata)
{
  redis_returnValue *rv;
  rv = redis_initReturnValue();
  if (rv == NULL) return NULL;
  rv->type = REDIS_RETURN_INTEGER;

  rv->integer = strtol(rdata, NULL, 0);
  rv->has_error = 0;
  return rv;
}

redis_returnValue *redis_parseBulkReturn(char* rdata)
{
  redis_returnValue *rv;
  int bulklen;
  char *tail;
  rv = redis_initReturnValue();
  if (rv == NULL) return NULL;
  bulklen = strtol(rdata, &tail, 0);
  rv->type = REDIS_RETURN_BULK;
  if (bulklen == -1) return rv;
  tail += 2;
  rv->bulk = (char *)malloc((bulklen+1) * sizeof(char));
  if (rv->bulk == NULL)
  {
    redis_freeResult(rv);
    return NULL;
  }
  rv->bulk[bulklen] = '\0';
  rv->bulk = strncpy(rv->bulk, tail, bulklen);
  rv->has_error = 0;
  return rv;
}

redis_returnValue *redis_parseMultiBulkReturn(char* rdata)
{
  redis_returnValue *rv;
  int  bulklen;
  char *tail;
  int i;

  rv = redis_initReturnValue();
  if (rv == NULL) return NULL;
  rv->type = REDIS_RETURN_MULTIBULK;
  rv->multibulkSize = strtol(rdata, &tail, 0);
  if (rv->multibulkSize == -1)
    return rv;
  rv->multibulk = (char **)malloc((rv->multibulkSize+1) * sizeof(char*));
  if (rv->multibulk == NULL)
  {
    redis_freeResult(rv);
    return NULL;
  }
  tail += 2;
  i = 0;
  while (i < rv->multibulkSize)
  {
    tail++;
    bulklen = strtol(tail, &tail, 0);
    tail += 2;
    if (bulklen == -1)
    {
      rv->multibulk[i] = NULL;
      i++;
      continue;
    }
    rv->multibulk[i] = (char *)malloc((bulklen+1) * sizeof(char));
    if (rv->multibulk[i] == NULL)
    {
      redis_freeResult(rv);
      return NULL;
    }
    rv->multibulk[i][bulklen] = '\0';
    rv->multibulk[i]          = strncpy(rv->multibulk[i], tail, bulklen);
    tail += (bulklen + 2);
    i++;
  }

  rv->has_error = 0;
  return rv;
}

void redis_freeResult(redis_returnValue *rv)
{
  int i;

  if (rv->bulk      != NULL) free(rv->bulk);
  if (rv->errorMsg  != NULL) free(rv->errorMsg);
  if (rv->line      != NULL) free(rv->line);
  if (rv->multibulk != NULL)
  {
    for (i=0; i < rv->multibulkSize; i++)
      if (rv->multibulk[i] != NULL) free(rv->multibulk[i]);
    free(rv->multibulk);
  }
  free(rv);
}

redis_returnValue *redis_parseReturn(char *rdata)
{
  switch (*rdata)
  {
    case '-' : return redis_parseErrorReturn(++rdata);
    case '+' : return redis_parseLineReturn(++rdata);
    case '$' : return redis_parseBulkReturn(++rdata);
    case '*' : return redis_parseMultiBulkReturn(++rdata);
    case ':' : return redis_parseIntegerReturn(++rdata);
    default  : return NULL;
  }
}

redis_returnValue *redis_exec(REDIS *redis, redis_execType execType, ...)
{
  va_list           ap;
  int               argsc;
  char              **argsv;
  char              *arg;
  char              *cmd;
  char              *rdata;
  redis_returnValue *rv;
  int               rc;
  int               i;

  va_start(ap, execType);
  argsv = NULL;
  argsc = 0;

  arg = va_arg(ap, char*);
  if (arg == NULL) return NULL;
  while (arg != NULL)
  {
    argsc++;
    argsv = (char **) realloc(argsv, argsc * sizeof(char*));
    if (argsv == NULL) return NULL;
    argsv[argsc - 1] = strdup(arg);

    arg = va_arg(ap, char*);
  }
  va_end(ap);
  if (execType == REDIS_EXEC_DEFAULT)
    cmd = redis_buildCommand(argsc, argsv);
  else
    cmd = redis_genMultiBulk(argsc, argsv);

  for (i=0; i < argsc; i++)
    if (argsv[i] != NULL) free(argsv[i]);
  free(argsv);

  if (cmd == NULL) return NULL;
  rc = redis_send(redis, cmd, strlen(cmd));
  free(cmd);
  if (rc == -1) return NULL;

  rdata = redis_receive(redis);
  if (rdata == NULL) return NULL;
  rv = redis_parseReturn(rdata);
  free(rdata);
  return rv;
}
