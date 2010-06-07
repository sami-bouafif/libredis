/* redis.c
 *
 * Copyright (C) 2010
 *        Sami Bouafif <sami.bouafif@gmail.com>. All Rights Reserved.
 *
 * This file is part of libredis.
 *
 * libredis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libredis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <redis.h>

/**
 * SECTION:redis
 * @short_description: C library to interact with Redis server.
 * @title: libredis API
 * @section_id:
 * @include: redis.h
 *
 * libredis is a collection of functions and data structures that simplify the
 * interaction with Redis servers. Its main purpose is to bring to the end user
 * basic functionalities to use Redis to store and retrieve data:<sbr/>
 * - Connect to Redis server.<sbr/>
 * - Build the protocol string to send to server.<sbr/>
 * - Send and receive data.<sbr/>
 * - Translate the data received from Redis to exploitable structure.
 *
 * libredis can be considered as a "low level" API since it doesn't implement every
 * specific Redis command, it just give the user the possibility to send data
 * and retrieve response from the server. The data are usually a Redis
 * command sequence. Most of the validation of the data sent is done at the server
 * level and in case of errors, the response of the server will hold these errors.
 * Otherwise, the result of executing the command is returned.
 *
 * libredis functions usually return <code>NULL</code> or <code>-1</code> to
 * report errors and the error code will be stored <code>redis_errCode</code>.
 * If the error is related to standard library functions,
 * <code>redis_sysErrno</code> will also store the specific error code.<sbr/>
 * redisError_getStr() and redisError_getSysErrorStr() can be used to get error
 * details and their return values should never be freed.
 *
 * <example>
 * <title>Example using libredis.</title>
 * <programlisting>
 * #include <stdio.h>
 * #include <redis.h>
 *
 * void printResult(RedisRetVal *rv)
 * {
 *    int i;
 *    switch (rv->type)
 *    {
 *        case REDIS_RETURN_INTEGER:
 *            printf("Return Type  : Integer\nReturn Value : %d\n", rv->integer);
 *            break;
 *        case REDIS_RETURN_LINE:
 *            printf("Return Type  : Line\nReturn Value : %B\n", rv->line);
 *            break;
 *        case REDIS_RETURN_ERROR:
 *            printf("Return Type  : error\nReturn Value : %B\n", rv->errorMsg);
 *            break;
 *        case REDIS_RETURN_BULK:
 *            printf("Return Type  : bulk\nReturn Value : %B\n", rv->bulk);
 *            break;
 *        case REDIS_RETURN_MULTIBULK:
 *            printf("Return Type  : multibulk\nReturn Value :\n");
 *            for (i = 0; i < rv->multibulkSize; i++)
 *                printf("\t%B\n", rv->multibulk[i]);
 *            printf("\n");
 *            break;
 *    }
 * }
 *
 * int main(int argc, char **argv)
 * {
 *    REDIS *redis;
 *    RedisCmd *cmd;
 *    RedisRetVal *rv;
 *
 *    if ((redis = redis_connect(NULL, 0)) == NULL)
 *    {
 *        printf("    Error : %s\n", redisError_getStr(redis_errCode));
 *        printf("SYS Error : %s\n", redisError_getSysErrorStr(redis_errCode,
 *                                                             redis_sysErrno));
 *        return -1;
 *    }
 *
 *    cmd = redisCmd_new(REDIS_PROTOCOL_MULTIBULK, "SET");
 *    redisCmd_addArg(cmd, "key1", -1);
 *    redisCmd_addArg(cmd, "A Value", -1);
 *    rv = redisCmd_exec(redis, cmd);
 *    printResult(rv);
 *    redisCmd_free(cmd);
 *
 *    cmd = redisCmd_newFromStr(REDIS_PROTOCOL_OLD, "SET key2 'Another Value'", -1);
 *    printf("Protocol string :\n%B \n", redisCmd_buildProtocolStr(cmd));
 *    rv = redisCmd_exec(redis, cmd);
 *    printResult(rv);
 *    redisCmd_free(cmd);
 *
 *    rv = redis_execStr(redis, REDIS_PROTOCOL_MULTIBULK, "GET key2", -1);
 *    printResult(rv);
 *    redisRetVal_free(rv);
 *
 *    redis_close(redis);
 *    return 0;
 * }
 * </programlisting>
 * </example>
 **/

/* Basic structure */

#define MAXDATASIZE  1024
#define MAXSTRLENGTH 1024

struct _REDIS
{
  int   fd;                     /* socket descriptor to Redis Server      */
  char  host[INET6_ADDRSTRLEN]; /* Redis server host                      */
  char  *port;                  /* Redis server port, service name or num */
  int   lasterror;              /* Last error                             */
  char  *errorstr;              /* Error details                          */
};

struct _RedisCmd
 {
   RedisProtocolType   protocolType;
   bstr_t              *args;
   int                 argsCount;
   bstr_t              protocolString;
   RedisRetVal         *returnValue;
 };

/* Description entry of an errorCode */
typedef struct
{
  int         errCode;
  const char  *errStr;
} RedisErrorSpec;

/* List of error descriptions */
static RedisErrorSpec redisErrorSpecTable[] =
{
  {REDIS_NOERROR,               "All is OK."                           },
  {REDIS_ERROR_MEM_ALLOC,       "Error allocating memory."             },
  {REDIS_ERROR_CNX_SOCKET,      "Error creating socket."               },
  {REDIS_ERROR_CNX_CONNECT,     "Unable to connect to server."         },
  {REDIS_ERROR_CNX_TIMEOUT,     "Connection timeout."                  },
  {REDIS_ERROR_CNX_SEND,        "Error sending data."                  },
  {REDIS_ERROR_CNX_RECEIVE,     "Error receiving data."                },
  {REDIS_ERROR_CNX_GAI,         "Error getting address info."          },
  {REDIS_ERROR_CMD_UNKNOWN,     "Unknown redis command."               },
  {REDIS_ERROR_CMD_ARGS,        "Invalid arguments."                   },
  {REDIS_ERROR_CMD_INVALID,     "Invalid command structure."           },
  {REDIS_ERROR_CMD_UNBALANCEDQ, "Unbalanced quotes in command string." },
  {-1, NULL}
};

/**
 * redisError_getStr:
 * @errorCode: libredis error code.
 *
 * Get the error description of an error code returned or set by a libredis
 * function.
 *
 * Returns: string containing the error description of @errorCode or <code>NULL</code> if
 * no description found.
 **/
const char* redisError_getStr(redisErrorCode errorCode)
{
  int i = 0;
  while(redisErrorSpecTable[i].errStr != NULL)
  {
    if (errorCode == redisErrorSpecTable[i].errCode)
      return redisErrorSpecTable[i].errStr;
    i++;
  }
  return NULL;
}

/**
 * redisError_getSysErrorStr:
 * @errorCode:  libredis error code.
 * @sysErrCode: standard library error code.
 *
 * Get the error description corresponding to a standard library error (usually
 * <code>errno</code>). This function is usually used for debugging purpose.
 *
 * Returns: string containing the error description of a standard library error.
 **/
const char* redisError_getSysErrorStr(redisErrorCode errorCode, int sysErrCode)
{
  if (errorCode == REDIS_ERROR_CNX_GAI) return gai_strerror(sysErrCode);
  return strerror(sysErrCode);
}
/* Functions to set errors */

/* Set mem allocation error */
static int _redis_setMallocError()
{
  redis_errCode = REDIS_ERROR_MEM_ALLOC;
  redis_sysErrno = errno;
  return REDIS_ERROR_MEM_ALLOC;
}

/* Set connection error */
static int _redis_setCnxError(int errorCode, int sysErrno)
{
  redis_errCode = errorCode;
  redis_sysErrno = sysErrno;
  return errorCode;
}

/* set Redis server related error */
static int _redis_setSrvError(int errorCode)
{
  redis_errCode = errorCode;
  redis_sysErrno = 0;
  return errorCode;
}

/* Taken +/- verbatim from rediscli */
/* Possible Types of a Redis command */
enum
{
  REDIS_CMD_INLINE,
  REDIS_CMD_BULK,
  REDIS_CMD_MULTIBULK
};

/* Description of a Redis command */
struct RedisCmdSpec
{
  char *name;
  int  arity;
  int  flags;
};

/* List of Redis commands (relative to version 1.2.6) */
static struct RedisCmdSpec redisCommandSpecTable[] = {
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

/* Retrieve the spec of a given Redis command name */
static struct RedisCmdSpec* _redis_lookupCommandSpec(char *name)
{
  int i = 0;
  while(redisCommandSpecTable[i].name != NULL)
  {
    if (!strcasecmp(name,redisCommandSpecTable[i].name)) return &redisCommandSpecTable[i];
    i++;
  }
  return NULL;
}

/*
 * Close connection and free memory
 */
static void _redis_free(REDIS *redis)
{
  if (redis == NULL) return;
  close(redis->fd);
  if (redis->port) free(redis->port);
  free(redis);

}

/**
 * redis_connect:
 * @host: host to connect to or <code>NULL</code>.
 * @port: port to connect to or <code>NULL</code>.
 *
 * Connect to Redis server @host at port @port. If @host or/and @port are <code>NULL</code>,
 * the default values will be used ("127.0.0.1" or/and "6379").<sbr/>
 * @host can be a host name or address and @port can be a port number or a
 * port name ("http", "ftp", ...)
 *
 * Returns: a REDIS struct or <code>NULL</code> on error. <code>redis_errCode</code> will hold
 * the error code. redisError_getStr() can be used to retrieve the error details.
 **/
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
  int    rc;

  redis = (REDIS*) malloc(sizeof(REDIS));
  if (redis == NULL)
  {
    _redis_setMallocError();
    return NULL;
  }

  redis->port = NULL;
  servername = host ? host
                    : "127.0.0.1";
  serverport = port ? port
                    : "6379";
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  /* Get address informations of the server */
  rc = getaddrinfo(servername, serverport, &hints, &servinfo);
  if ( rc != 0)
  {
    _redis_free(redis);
    _redis_setCnxError(REDIS_ERROR_CNX_GAI, rc);
    return NULL;
  }

  /*
   * Loop through the returned addresses and try to connect to one of them.
   * stop looping after the first successful connection.
   */
  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    redis->fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

    if (redis->fd == -1)
    {
      _redis_setCnxError(REDIS_ERROR_CNX_SOCKET, errno);
      continue;
    }

    /* Set socket options */
    if (setsockopt(redis->fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval) == -1||
        setsockopt(redis->fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval) == -1)
    {
      freeaddrinfo(servinfo);
      _redis_free(redis);
      _redis_setCnxError(REDIS_ERROR_CNX_SOCKET, errno);
      return NULL;
    }
    if (connect(redis->fd, p->ai_addr, p->ai_addrlen) == -1)
      _redis_setCnxError(REDIS_ERROR_CNX_CONNECT, errno);
    else
      break;
  }

  /*
   * p will hold a NULL pointer if the connection has failed.
   * return NULL in this case after freeing allocated memory.
   */
  if (p == NULL)
  {
    freeaddrinfo(servinfo);
    _redis_free(redis);
    return NULL;
  }

  /* Get the address of the server depending of the its address family */
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
    _redis_setMallocError();
    freeaddrinfo(servinfo);
    _redis_free(redis);
    return NULL;
  }
  /*
   * Get the real address of the server.
   * We can use the address specified by the user, but the address returned
   * by getaddrinfo is more appropriate.
   */
  inet_ntop(p->ai_family, addr, redis->host, sizeof(redis->host));
  strcpy(redis->port, serverport);
  freeaddrinfo(servinfo);
  return redis;
}

/*
 * Send data to Redis server
 * return :
 *    - REDIS_ERROR_CNX_SEND on error.
 *    - REDIS_ERROR_CNX_TIMEOUT on timeout.
 *    - REDIS_NOERROR on success.
 */
static int _redis_send(REDIS *redis, bstr_t data)
{
  size_t sent;
  size_t n;
  fd_set fds;
  struct timeval tv;
  int     rc;

  /*
   * timeval is set to 10sec.
   * I think it's useless to give the user the possibility to specify this value.
   */
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  sent = 0;
  rc = 0;

  while (sent < bstr_len(data))
  {
    FD_ZERO(&fds);
    FD_SET(redis->fd, &fds);
    rc = select(redis->fd+1, NULL, &fds, NULL, &tv);
    if (rc <= 0) break;
    n = send(redis->fd, (char *)data + sent, bstr_len(data)-sent, 0);
    if (n == -1) return _redis_setCnxError(REDIS_ERROR_CNX_SEND, errno);
    sent += n;
  }
  if (rc == 0)  return _redis_setCnxError(REDIS_ERROR_CNX_TIMEOUT, 0);
  if (rc == -1) return _redis_setCnxError(REDIS_ERROR_CNX_SEND, errno);
  return REDIS_NOERROR;
}

/*
 * Receive data from Redis server
 *
 * return NULL on error and set redis_errCode accordingly.
 */
static bstr_t _redis_receive(REDIS *redis)
{
  char buffer[MAXDATASIZE];
  bstr_t data = NULL;
  int n;
  fd_set fds;
  struct timeval tv;
  int rc;

  /*
   * timeval is set to 10sec.
   * I think it's useless to give the user the possibility to specify this value.
   */
  tv.tv_sec = 10;
  tv.tv_usec = 0;

  /*init data*/
  data = bstr_new(NULL, 0);
  if (data == NULL)
  {
    _redis_setMallocError();
    return NULL;
  }

  while (1)
  {
    /* Prepare the file description */
    FD_ZERO(&fds);
    FD_SET(redis->fd, &fds);
    rc = select(redis->fd+1, &fds, NULL, NULL, &tv);
    if (rc <= 0)
    {
      (rc == 0) ? _redis_setCnxError(REDIS_ERROR_CNX_TIMEOUT, 0)
                : _redis_setCnxError(REDIS_ERROR_CNX_RECEIVE, errno);
      free(data);
      return NULL;
    }

    /* Reveive data */
    n = recv(redis->fd, buffer, MAXDATASIZE, 0);
    if (n == -1)
    {
      _redis_setCnxError(REDIS_ERROR_CNX_RECEIVE, errno);
      free(data);
      return NULL;
    }
    if (n ==  0) return data;
    data = bstr_cat(data, buffer, n);
    if (data == NULL)
    {
      _redis_setMallocError();
      return NULL;
    }
    if (n < MAXDATASIZE) return data;
  }
  return NULL;
}

/**
 * redis_close:
 * @redis: target #REDIS structure to close.
 * 
 * Close connection to the server and free the memory allocated by the structure.
 * The connection will be maintained until this function is called.
 **/
void redis_close(REDIS *redis)
{
  _redis_free(redis);
}

/**
 * redisCmd_new:
 * @protocolType: type of protocol to use.
 * @cmdName: Redis command or <code>NULL</code>.
 *
 * Create a new command structure for cmdName according to @protocolType protocol.
 * If cmdName is <code>NULL</code>, build an empty command structure.
 * <note>
 * %REDIS_PROTOCOL_OLD specify the protocol used in early versions of Redis and
 * will probably be deprecated. Starting from version 1.1, it is recommended to
 * use %REDIS_PROTOCOL_MULTIBULK.
 * </note>
 *
 * Returns: the newly allocated #RedisCmd structure or <code>NULL</code> on error (and
 * <code>redis_errCode</code> will hold the error code).
 **/
RedisCmd* redisCmd_new(RedisProtocolType protocolType, char *cmdName)
{
  RedisCmd *ret;
  struct RedisCmdSpec *redis_cmd;

  ret = (RedisCmd *)malloc(sizeof(RedisCmd));
  if (ret == NULL)
  {
    _redis_setMallocError();
    return NULL;
  }
  ret->protocolType   = protocolType;
  ret->argsCount      = 0;
  ret->args           = NULL;
  ret->protocolString = NULL;
  ret->returnValue    = NULL;

  if (cmdName == NULL) return ret;

  if (protocolType == REDIS_PROTOCOL_OLD)
  {
    redis_cmd = _redis_lookupCommandSpec(cmdName);
    if (!redis_cmd)
    {
      _redis_setSrvError(REDIS_ERROR_CMD_UNKNOWN);
      redisCmd_free(ret);
      return NULL;
    }
  }

  ret->argsCount++;
  ret->args = (bstr_t *)realloc(ret->args, ret->argsCount * sizeof(bstr_t));
  if (ret->args == NULL)
  {
    _redis_setMallocError();
    redisCmd_free(ret);
    return NULL;
  }
  if ((ret->args[ret->argsCount - 1] = bstr_newFromCStr(cmdName)) == NULL)
  {
    _redis_setMallocError();
    redisCmd_free(ret);
    return NULL;
  }
  return ret;

}

/*
 * Constructs a RedisCmd from a valid SQL string (respect SQL quotes)
 * return RedisCmd on success, NULL and set errCode on failure.
 */
static RedisCmd* _redisCmd_newFromSqlString(RedisProtocolType protocol,
                                            bstr_t bstr)
{
  RedisCmd      *ret;
  char          *str;
  char          *strEnd;
  char          *arg;
  int           arglen = 0;
  size_t        len;
  int           ignoreSplit = 0;

  str    = (char *)bstr;
  len    = bstr_len(bstr);
  arg    = (char*)calloc(len+1, sizeof(char)); /* Temp buffer to hold an arg */
  strEnd = str + len;                          /* The end of the string      */
  /*arg    = bstr_new(NULL, 0);*/
  ret    = redisCmd_new(protocol, NULL);   /* Construct a command struct */
  if (ret == NULL) return NULL;
  while(isspace(*str)) str++;                  /* Remove Leading spaces      */
  /*
   * loop to the end of the string. It may be replaced by while(1) since the
   * the exit from the loop happen with a break.
   */
  while(str <= strEnd)
  {
    if (*str =='\'')                           /* if current char is '       */
    {
      str++;                                   /* if the next char is '      */
      if (*str == '\'')
      {
        arg[arglen] = *str;                    /* append to buffer and loop  */
        arglen++;
        str ++;
        continue;                              /* continue to the next iter  */
      }
      ignoreSplit = !ignoreSplit;              /* if not, it mark the beginning
                                                * of a quote                 */
    }
    /* We split on whitespaces and the end of the string */
    if (!ignoreSplit && (isspace(*str) || str == strEnd))
    {
      /* store the current arg */
      if (redisCmd_addArg(ret, arg, arglen) != REDIS_NOERROR)
      {
        redisCmd_free(ret);
        free(arg);
        return NULL;
      }
      /* begin a new arg */
      arglen = 0;

      /* eat consecutif whitespaces */
      while(isspace(*str)) str++;
      /* if we reach the end of the string, we break out of the loop */
      if (str == strEnd) break;
      /* continue to the next iter */
      continue;
    }

    /*
     * If we reach the end of the string and a quote is not already close,
     * the quote is unbalanced, so report it and exit with error
     */
    if (ignoreSplit && str == strEnd)
    {
      _redis_setSrvError(REDIS_ERROR_CMD_UNBALANCEDQ);
      redisCmd_free(ret);
      free(arg);
      return NULL;
    }
    /* append the current char to the buffer and loop */
    arg[arglen] = *str;
    arglen++;
    str++;
  }
  free(arg);
  return ret;
}

/*

 *
 *
 * Return NULL on error and set errCode accordingly.
 *
 * NB. unbalanced quotes result in an REDIS_ERROR_CMD_UNBALANCEDQ
 */
/**
 * redisCmd_newFromStr:
 * @protocolType: protocol type to use.
 * @cmdStr: Redis command sequence.
 * @cmdLen: command length or <code>-1</code>
 *
 * Build a command from an SQL valid quoted string of length @cmdLen where:<sbr/>
 * - '  begin and end a quoted region. useful to embed whitespaces.<sbr/>
 * - '' is the escape sequence of '.<sbr/>
 * If @cmdLen is <code>-1</code>, <code>strlen</code> is used to get the length
 * of @cmdStr.
 * 
 * It splits @cmdStr to tokens on whitespaces(unless quoted). These tokens will
 * form the args of the #RedisCmd structure returned.
 *
 * Returns: Return a #RedisCmd struct or <code>NULL</code> on error and set
 * <code>redis_errCode</code> accordingly.
 **/
RedisCmd* redisCmd_newFromStr(RedisProtocolType protocolType,
                              char *cmdStr,
                              int  cmdLen)
{
  RedisCmd *ret;
  bstr_t   cmdBStr;

  cmdBStr = bstr_new(cmdStr, cmdLen);
  if (cmdBStr == NULL)
  {
    _redis_setMallocError();
    bstr_free(cmdBStr);
    return NULL;
  }
  ret = _redisCmd_newFromSqlString(protocolType, cmdBStr);
  bstr_free(cmdBStr);
  if (ret == NULL) return NULL;

  return ret;
}

/**
 * redisCmd_addArg:
 * @cmd: #RedisCmd to add @arg to.
 * @arg: the arg to add.
 * @arglen: length of @arg.
 *
 * Add the argument @arg of length @arglen to an existing #RedisCmd structure.
 * if @arglen is <code>-1</code>, <code>strlen</code> is used to get the length
 * of @arg.
 * Returns: %REDIS_NOERROR if all is ok, else the error code corresponding to
 * the error.
 **/
int redisCmd_addArg(RedisCmd *cmd, char *arg, size_t arglen)
{
  if (cmd == NULL) return REDIS_ERROR_CMD_INVALID;
  if (arg == NULL) return REDIS_ERROR_CMD_ARGS;

  cmd->argsCount++;
  cmd->args = (bstr_t *)realloc(cmd->args, cmd->argsCount * sizeof(bstr_t));
  if (cmd->args == NULL)
    return _redis_setMallocError();
  if ((cmd->args[cmd->argsCount - 1] = bstr_new(arg, arglen)) == NULL)
    return _redis_setMallocError();
  return REDIS_NOERROR;
}

/**
 * redisCmd_free:
 * @cmd: #RedisCmd to free.
 *
 * Free the memory allocated to @cmd. It is advised to call redisCmd_free() on
 * #RedisCmd structures no longer used.
 */
void redisCmd_free(RedisCmd *cmd)
{
  int i;
  if (cmd == NULL) return;
  if(cmd->protocolString != NULL) bstr_free(cmd->protocolString);
  if(cmd->returnValue != NULL) redisRetVal_free(cmd->returnValue);
  if (cmd->args != NULL)
  {
    for(i = 0; i < cmd->argsCount; i++)
      if (cmd->args[i] != NULL) bstr_free(cmd->args[i]);
    free(cmd->args);
  }
  free(cmd);
}

/*
 * Generate a multibulk command.
 * return the generated command according to Redis protocol or NULL on error.
 */
static bstr_t _redisCmd_genMultiBulk (RedisCmd *cmd)
{
  bstr_t  *args;
  int     rc, i;

  args = cmd->args;
  rc = bstr_asprintf(&cmd->protocolString, "*%d\r\n", cmd->argsCount);
  if (rc < 0)
  {
    _redis_setMallocError();
    return NULL;
  }
  for (i=0; i < cmd->argsCount; i++)
  {
    rc = bstr_scatprintf(&cmd->protocolString, "$%lu\r\n%B\r\n", bstr_len(args[i]), args[i]);
    if (rc < 0)
    {
      _redis_setMallocError();
      return NULL;
    }
  }
  return cmd->protocolString;
}

/*
 * Generate a bulk command.
 * return the generated command according to Redis protocol or NULL on error.
 */
static bstr_t _redisCmd_genBulk (RedisCmd *cmd)
{
  bstr_t *args;
  int     rc, i;

  args = cmd->args;
  for (i=0; i < cmd->argsCount - 1; i++)
  {
    rc = bstr_scatprintf(&cmd->protocolString, "%B ", args[i]);
    if (rc < 0)
    {
      _redis_setMallocError();
      return NULL;
    }
  }

  rc = bstr_scatprintf(&cmd->protocolString, "%lu\r\n%B\r\n",
                       bstr_len(args[cmd->argsCount - 1]),
                       args[cmd->argsCount - 1]);
  if (rc < 0)
  {
    _redis_setMallocError();
    return NULL;
  }
  return cmd->protocolString;
}

/*
 * Generate a bulk command.
 * return the generated command according to Redis protocol or NULL on error.
 */
static bstr_t _redisCmd_genInline (RedisCmd *cmd)
{
  bstr_t  *args;
  int     rc, i;

  args = cmd->args;
  for (i=0; i < cmd->argsCount; i++)
  {
    rc = bstr_scatprintf(&cmd->protocolString, "%B ", args[i]);
    if (rc < 0)
    {
      _redis_setMallocError();
      return NULL;
    }
  }
  rc = bstr_scatprintf(&cmd->protocolString, "\r\n");
  if (rc < 0)
  {
    _redis_setMallocError();
    return NULL;
  }
  return cmd->protocolString;
}

/* Initialize Redis return value structure */
static RedisRetVal* _redis_initReturnValue()
{
  RedisRetVal *rv;
  rv = (RedisRetVal *) malloc(sizeof(RedisRetVal));
  if (rv == NULL)
  {
    _redis_setMallocError();
    return NULL;
  }
  rv->bulk = NULL;
  rv->errorMsg = NULL;
  rv->multibulk = NULL;
  rv->line = NULL;
  return rv;
}

/*
 * parse an error returned by Redis server and make the corresponding RedisRetVal.
 * The value returned is the same as
 * return NULL on error.
 */
static RedisRetVal* _redisRetVal_parseError(char *rdata)
{
  RedisRetVal *rv;
  char        *p = rdata;

  rv = _redis_initReturnValue();
  if (rv == NULL) return NULL;
  rv->type = REDIS_RETURN_ERROR;
  /*
   * Real rdata address is at rdata -1.
   * To get the length of data to copy, we must get the real length of rdata
   * (bstr_len(rdata -1)). The target length is the real rdata len minus 3
   * (the length of '-', '\r' and '\n') : bstr_len(rdata -1) -3
   */

  while(strcmp(p, "\r\n") != 0) p++;
  rv->errorMsg = bstr_new(rdata, p - rdata);
  if (rv->errorMsg == NULL)
  {
    _redis_setMallocError();
    redisRetVal_free(rv);
    return NULL;
  }

  return rv;
}

/*
 * parse a line returned by Redis server and make the corresponding RedisRetVal.
 * return NULL on error.
 */
static RedisRetVal* _redisRetVal_parseLine(char *rdata)
{
  RedisRetVal *rv;
  char        *p = rdata;

  rv = _redis_initReturnValue();
  if (rv == NULL) return NULL;
  rv->type = REDIS_RETURN_LINE;
  /*
   * Real rdata address is at rdata -1.
   * To get the length of data to copy, we must get the real length of rdata
   * (bstr_len(rdata -1)). The target length is the real rdata len minus 3
   * (the length of '+', '\r' and '\n') : bstr_len(rdata -1) -3
   */
  while(strcmp(p, "\r\n") != 0) p++;
  rv->line = bstr_new(rdata, p - rdata);
  if (rv->line == NULL)
  {
    _redis_setMallocError();
    redisRetVal_free(rv);
    return NULL;
  }

  return rv;
}

/*
 * parse an int returned by Redis server and make the corresponding RedisRetVal.
 * return NULL on error.
 */
static RedisRetVal* _redisRetVal_parseInteger(char *rdata)
{
  RedisRetVal *rv;

  rv = _redis_initReturnValue();
  if (rv == NULL) return NULL;
  rv->type = REDIS_RETURN_INTEGER;

  /* bstr_t can be used as char* */
  rv->integer = strtol(rdata, NULL, 0);

  return rv;
}

/*
 * parse a bulk returned by Redis server and make the corresponding RedisRetVal.
 * return NULL on error.
 */
static RedisRetVal* _redisRetVal_parseBulk(char *rdata)
{
  RedisRetVal *rv;
  int               bulklen;
  char              *tail;
  rv = _redis_initReturnValue();
  if (rv == NULL) return NULL;
  bulklen = strtol(rdata, &tail, 0);
  rv->type = REDIS_RETURN_BULK;
  if (bulklen == -1) return rv;
  tail += 2;
  rv->bulk = bstr_new(tail, bulklen);
  if (rv->bulk == NULL)
  {
    _redis_setMallocError();
    redisRetVal_free(rv);
    return NULL;
  }

  return rv;
}

/*
 * parse a multibulk returned by Redis server and make the corresponding RedisRetVal.
 * return NULL on error.
 */
static RedisRetVal* _redisRetVal_parseMultiBulk(char *rdata)
{
  RedisRetVal *rv;
  int         bulklen;
  char        *tail;
  int         i;

  rv = _redis_initReturnValue();
  if (rv == NULL) return NULL;
  rv->type = REDIS_RETURN_MULTIBULK;
  /* Get array size (multibuk size). If -1, an error, return a NULL multibulk. */
  rv->multibulkSize = strtol(rdata, &tail, 0);
  if (rv->multibulkSize == -1)
    return rv;
  /* Allocate necessary space to store data of size rv->multibulkSize */
  rv->multibulk = (bstr_t *)calloc(rv->multibulkSize, sizeof(bstr_t));
  if (rv->multibulk == NULL)
  {
    _redis_setMallocError();
    redisRetVal_free(rv);
    return NULL;
  }
  tail += 2;                          /* Step through "\r\n" */
  i = 0;
  while (i < rv->multibulkSize)       /* For each element in multibulk data */
  {
    tail++;                           /* Step through $ */
    bulklen = strtol(tail, &tail, 0); /* Get an element length */
    tail += 2;                        /* Step through "\r\n" */
    if (bulklen == -1)                /* If the element length is -1 then element is NULL */
    {
      rv->multibulk[i] = NULL;
      i++;
      continue;
    }
    /* Store the value of size bulklen starting from tail */
    rv->multibulk[i] = bstr_new(tail, bulklen);
    if (rv->multibulk[i] == NULL)
    {
      _redis_setMallocError();
      redisRetVal_free(rv);
      return NULL;
    }
    tail += (bulklen + 2);            /* Jump to the end of data bypassing "\r\n" */
    i++;
  }

  return rv;
}

/*
 *
 */
/**
 * redisCmd_buildProtocolStr:
 * @cmd: #RedisCmd structure to build from the string.
 *
 * Build the command string according to the Redis protocol specified in
 * #RedisCmd.protocolType. This string will be sent to Redis server when @cmd
 * is executed.<sbr/>
 * It is not required to build the #RedisCmd.protocolType manually since it is
 * generated when the @cmd is about to be executed. However redisCmd_buildProtocolStr()
 * can be useful in debugging tasks.
 *
 * Returns: the string to send to Redis server on success or <code>NULL</code>
 * on error and <code>redis_errCode</code> is set accordingly.
 **/
bstr_t redisCmd_buildProtocolStr(RedisCmd *cmd)
{
  struct RedisCmdSpec *cmdSpec;

  if (cmd == NULL)
  {
    _redis_setSrvError(REDIS_ERROR_CMD_INVALID);
    return NULL;
  }

  if (cmd->protocolType == REDIS_PROTOCOL_MULTIBULK)
    return _redisCmd_genMultiBulk(cmd);

  cmdSpec = _redis_lookupCommandSpec(*(char **)cmd->args);

  if ((cmdSpec->arity > 0 && cmd->argsCount != cmdSpec->arity) ||
      (cmdSpec->arity < 0 && cmd->argsCount < -cmdSpec->arity)   )
  {
    _redis_setSrvError(REDIS_ERROR_CMD_ARGS);
    return NULL;
  }

  switch (cmdSpec->flags)
  {
    case REDIS_CMD_MULTIBULK : return _redisCmd_genMultiBulk(cmd);
    case REDIS_CMD_BULK      : return _redisCmd_genBulk(cmd);
    case REDIS_CMD_INLINE    : return _redisCmd_genInline(cmd);
  }
  return NULL;
}


/**
 * redisRetVal_free:
 * @rv: #RedisRetVal structure to free.
 * 
 * Free the memory allocated to @rv. It is recommended to call redisRetVal_free()
 * on #RedisRetVal structures returned by redis_exec() and redis_execStr()
 * when they are no longer used.
 **/
void redisRetVal_free(RedisRetVal *rv)
{
  int i;

  if (rv->bulk      != NULL) bstr_free(rv->bulk);
  if (rv->errorMsg  != NULL) bstr_free(rv->errorMsg);
  if (rv->line      != NULL) bstr_free(rv->line);
  if (rv->multibulk != NULL)
  {
    for (i=0; i < rv->multibulkSize; i++)
      if (rv->multibulk[i] != NULL) bstr_free(rv->multibulk[i]);
    free(rv->multibulk);
  }
  free(rv);
}

/*
 * Return the appropriate returnValue from string reveived from Redis.
 */
static RedisRetVal* _redisRetVal_parse(bstr_t rdata, char **tail)
{
  /* TODO : change this. use char *data = (char *)rdata */
  char *data = (char *)rdata;
  switch (*data)
  {
    case '-' : return _redisRetVal_parseError(++data);
    case '+' : return _redisRetVal_parseLine(++data);
    case '$' : return _redisRetVal_parseBulk(++data);
    case '*' : return _redisRetVal_parseMultiBulk(++data);
    case ':' : return _redisRetVal_parseInteger(++data);
    default  : return NULL;
  }
}

/* Exec a command and return the corresponding returnValue.
 * There is 2 ways to exec the command (depending on cmd->protocolType):
 * - The old way (using the redis_commandSpecTable)
 * - The new way (using the multibulk command format)
 *
 * return NULL on error.
 */
/**
 * redisCmd_exec:
 * @redis: #REDIS structure to use.
 * @cmd: #RedisCmd structure to execute
 *
 * Execute @cmd by sending the corresponding #RedisCmd.protocolString and
 * receiving the response from Redis server described by @redis structure. This
 * response will stored in #RedisCmd.returnValue.
 *
 * Returns: a #RedisRetVal structure containing the response of the server or
 * <code>NULL</code> on error and <code>redis_errCode</code> is set accordingly.
 * Note that the return value should never be freed since it is the same as
 * #RedisCmd.returnValue and it will be freed when @cmd is freed.
 **/
RedisRetVal* redisCmd_exec(REDIS *redis, RedisCmd *cmd)
{
  bstr_t            rdata;
  RedisRetVal       *rv;
  int               rc;

  if (cmd->protocolString == NULL)
    if (redisCmd_buildProtocolStr(cmd) == NULL) return NULL;

  rc = _redis_send(redis, cmd->protocolString);
  if (rc != REDIS_NOERROR) return NULL;

  rdata = _redis_receive(redis);
  if (rdata == NULL) return NULL;
  rv = _redisRetVal_parse(rdata, NULL);
  cmd->returnValue = rv;
  bstr_free(rdata);
  return rv;
}

/**
 * redisCmd_reset:
 * @cmd: RedisCmd to reset.
 * @cmdName: Redis command name.
 *
 * Reset @cmd so it can be reused. This is the same as as redisCmd_new() but does
 * not allocate memory. It just reuse the already allocated zone.
 *
 * Returns: return %REDIS_NOERROR on success or the corresponding error code on
 * error.
 **/
int redisCmd_reset(RedisCmd *cmd, char *cmdName)
{
  int i;

  if (cmd->returnValue != NULL)
  {
    redisRetVal_free(cmd->returnValue);
    cmd->returnValue = NULL;
  }
  if (cmd->protocolString != NULL)
  {
    bstr_free(cmd->protocolString);
    cmd->protocolString = NULL;
  }
  if (cmd->args != NULL)
  {
    for(i = 0; i < cmd->argsCount; i++)
      if (cmd->args[i] != NULL) bstr_free(cmd->args[i]);
    free(cmd->args);
  }
  cmd->argsCount = 0;
  if (cmdName != NULL)
  {
    return redisCmd_addArg(cmd, cmdName, -1);
  }
  return REDIS_NOERROR;
}

/*
 *
 * return REDIS_NOERROR if ok, -1 if the protocol string cannot be generated.
 */
/**
 * redisCmd_setProtocolType:
 * @cmd: the #RedisCmd structure to modify.
 * @protocol: the protocol to use.
 *
 * Set the protocol type of @cmd to @protocol.
 * If the protocol string is already generated, it refreshen it to reflect the
 * protocol type specified.
 *
 * Returns: REDIS_NOERROR on success or <code>-1</code> if the protocol string
 * cannot be generated.
 **/
int redisCmd_setProtocolType(RedisCmd *cmd, RedisProtocolType protocol)
{
  cmd->protocolType = protocol;
  if(redisCmd_buildProtocolStr(cmd) == NULL) return -1;
  return REDIS_NOERROR;
}

/*
 *
 */
/**
 * redisCmd_getRetVal:
 * @cmd: a #RedisCmd structure.
 *
 * Get the return value of a @cmd or <code>NULL</code> If the command has not
 * been executed.
 *
 * Returns: #RedisCmd.returnValue of @cmd.
 **/
RedisRetVal* redisCmd_getRetVal(RedisCmd *cmd)
{
  return cmd->returnValue;
}
/*
 *
 *
 * Return a pointer to RedisRetVal structure or NULL on error. errorCode
 * is set accordingly.
 *
 * NB.
 */

/**
 * redis_exec:
 * @redis: #REDIS structure to use.
 * @protocol: the protocol to use to communicate with Redis server.
 * @cmdName: the Redis command to execute.
 * @...: <code>NULL</code> terminated list of arg-argLen pairs.
 *
 * Execute the command described by @cmdName and the list of args. Last element
 * of the list must be a literal <code>NULL</code>. The arguments must be in the
 * form:
 * <informalexample><programlisting>
 * redis_exec(redis, protocol, cmdName, arg1, arglen1,
 *                                      arg2, arglen2,
 *                                      ...,
 *                                      NULL);
 * </programlisting></informalexample>
 * otherwise the behavior is unpredictable.
 *
 * This is a shortcut to the sequence of the following functions:<sbr/>
 * - redisCmd_new()<sbr/>
 * - redisCmd_addArg() (optional depending of the command arguments).<sbr/>
 * - redisCmd_buildProtocolStr().<sbr/>
 * - redisCmd_exec().<sbr/>
 *
 * Returns: a #RedisRetVal structure or <code>NULL</code> on error and
 * <code>redis_errCode</code> is set accordingly. It is recommended to free
 * the return value with redisRetVal_free() when no longer needed.
 **/
RedisRetVal* redis_exec(REDIS *redis,
                        RedisProtocolType protocol,
                        char *cmdName,
                        ...)
{
  va_list           ap;
  RedisCmd          *cmd;
  RedisRetVal       *ret;
  bstr_t            rdata;
  char              *arg;
  int               arglen;

  if (cmdName == NULL)
  {
    _redis_setSrvError(REDIS_ERROR_CMD_INVALID);
    return NULL;
  }
  cmd = redisCmd_new(protocol, cmdName);
  if (cmd == NULL) return NULL;

  va_start(ap, cmdName);
  while ((arg = va_arg(ap, char *)) != NULL)
  {
    arglen = va_arg(ap, int);
    if (arglen == 0)
    {
      _redis_setSrvError(REDIS_ERROR_CMD_ARGS);
      redisCmd_free(cmd);
      va_end(ap);
      return NULL;
    }
    arglen = (arglen == -1) ? strlen(arg)
                            : arglen;
    printf("ARG    : %s\nARGLEN : %d\n", arg, arglen);
    if (redisCmd_addArg(cmd, arg, arglen) != REDIS_NOERROR)
    {
      redisCmd_free(cmd);
      va_end(ap);
      return NULL;
    }
  }
  va_end(ap);

  if (redisCmd_buildProtocolStr(cmd) == NULL)
  {
    redisCmd_free(cmd);
    return NULL;
  }

  if(_redis_send(redis, cmd->protocolString) != REDIS_NOERROR)
  {
    redisCmd_free(cmd);
    return NULL;
  }

  if ((rdata = _redis_receive(redis)) == NULL)
  {
    redisCmd_free(cmd);
    return NULL;
  }
  ret = _redisRetVal_parse(rdata, NULL);

  redisCmd_free(cmd);
  bstr_free(rdata);

  return ret;
}

/**
 * redis_execStr:
 * @redis: #REDIS structure to use.
 * @protocol: the protocol to use to communicate with Redis server.
 * @cmdStr: the Redis command sequence to execute.
 * @cmdStrLen: @cmdStr length or <code>-1</code>
 *
 * Execute @cmdStr on the server and return the result as a #RedisRetVal structure.
 * @cmdStr and @cmdStrLen meet the same requirement as those passed to
 * redisCmd_newFromStr().
 *
 * In fact, redis_execStr() is a shortcut to the following call sequence:<sbr/>
 * - redisCmd_newFromStr().<sbr/>
 * - redisCmd_buildProtocolStr().<sbr/>
 * - redisCmd_exec().<sbr/>
 *
 * 
 *
 *
 * Returns: a  #RedisRetVal structure or <code>NULL</code> on error and
 * <code>redis_errCode</code> is set accordingly. It is recommended to free
 * the return value with redisRetVal_free() when no longer needed.
 **/
RedisRetVal* redis_execStr(REDIS *redis,
                           RedisProtocolType protocol,
                           char *cmdStr,
                           int cmdStrLen)
{
  RedisCmd    *cmd;
  RedisRetVal *ret;
  bstr_t      cmdBStr;
  bstr_t      rdata;

  cmdBStr = bstr_new(cmdStr, cmdStrLen);
  if (cmdBStr == NULL)
  {
    _redis_setMallocError();
    return NULL;
  }
  if ((cmd = _redisCmd_newFromSqlString(protocol, cmdBStr)) == NULL)
  {
    redisCmd_free(cmd);
    bstr_free(cmdBStr);
    return NULL;
  }
  bstr_free(cmdBStr);

  if (redisCmd_buildProtocolStr(cmd) == NULL)
  {
    redisCmd_free(cmd);
    return NULL;
  }

  if(_redis_send(redis, cmd->protocolString) != REDIS_NOERROR)
  {
    redisCmd_free(cmd);
    return NULL;
  }

  if ((rdata = _redis_receive(redis)) == NULL)
  {
    redisCmd_free(cmd);
    return NULL;
  }
  ret = _redisRetVal_parse(rdata, NULL);

  redisCmd_free(cmd);
  bstr_free(rdata);

  return ret;
}
