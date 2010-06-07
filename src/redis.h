/* redis.h
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

#ifndef REDIS_H_
#define REDIS_H_
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <bstr.h>
#include <redis.h>
/**
 * REDIS:
 *
 * This structure holds all informations about Redis server. It is used by
 * functions that communicate with the server.
 *
 * #REDIS is only created by redis_connect() and freed, when no longer needed,
 * by calling redis_close().
 **/
typedef struct _REDIS REDIS;

typedef enum
{
  REDIS_RETURN_ERROR,
  REDIS_RETURN_LINE,
  REDIS_RETURN_BULK,
  REDIS_RETURN_MULTIBULK,
  REDIS_RETURN_INTEGER
} RedisReturnType;

typedef enum
{
  REDIS_PROTOCOL_OLD,
  REDIS_PROTOCOL_MULTIBULK
} RedisProtocolType;

/**
 * RedisRetVal:
 * @type: type of the return value.
 * @errorMsg: if @type is %REDIS_RETURN_ERROR this var will hold the error
 * string or <code>NULL</code> if not.
 * @line: if @type is %REDIS_RETURN_LINE this var will hold the string returned
 * by Redis or <code>NULL</code> if not.
 * @bulk: if @type is %REDIS_RETURN_BULK this var will hold the data sequence
 * returned by Redis or <code>NULL</code> if not.
 * @multibulk: if @type is %REDIS_RETURN_MULTIBULK this var will hold the array
 * of data sequences returned by Redis or <code>NULL</code> if not.
 * @multibulkSize: if @type is %REDIS_RETURN_MULTIBULK this var will hold the
 * size of the array @multibulk.
 * @integer: f @type is %REDIS_RETURN_INTEGER this var will hold the integer
 * returned by Redis.
 *
 * Structure that holds the result of executing a Redis command. @type will
 * always contain the type of the result and the corresponding member the result.
 *
 * Note that #RedisRetVal is usually linked to a #RedisCmd structure and should
 * be freed manually (it will be freed when redisCmd_free() is called). However,
 * there is two exceptions to this rule, those returned by redis_exec() and
 * redis_execStr().
 * Those ones should be freed with redisRetVal_free() since they are not associated
 * with any #RedisCmd.
 **/
typedef struct _RedisRetVal RedisRetVal;
struct _RedisRetVal
{
  RedisReturnType  type;
  bstr_t           errorMsg;
  bstr_t           line;
  bstr_t           bulk;
  bstr_t           *multibulk;
  int              multibulkSize;
  int              integer;
};
/**
 * RedisCmd:
 *
 * This structure holds all informations concerning the command to be executed.
 * There is different ways to construct this structure (see <code>redisCmd_*</code>
 * functions) and the result can be passed to redisCmd_exec() to execute the
 * command.
 *
 * Although it is not required to construct a #RedisCmd to execute
 * a command (see redis_exec() and redis_execStr()), it is useful to observe
 * and fine tune the execution process on the server (add arguments interactively,
 * generating and visualizing the protocol string, reuse a command, ...).
 *
 * #RedisCmd should be freed with redisCmd_free() when it is no longer needed.
 **/
typedef struct _RedisCmd RedisCmd;

volatile int redis_errCode = 0;
/* errno set by standardlib functions */
volatile int redis_sysErrno = 0;

typedef enum
{
  REDIS_NOERROR,
  REDIS_ERROR_MEM_ALLOC,
  REDIS_ERROR_CNX_SOCKET,
  REDIS_ERROR_CNX_CONNECT,
  REDIS_ERROR_CNX_TIMEOUT,
  REDIS_ERROR_CNX_SEND,
  REDIS_ERROR_CNX_RECEIVE,
  REDIS_ERROR_CNX_GAI,
  REDIS_ERROR_CMD_UNKNOWN,
  REDIS_ERROR_CMD_ARGS,
  REDIS_ERROR_CMD_INVALID,
  REDIS_ERROR_CMD_UNBALANCEDQ
} redisErrorCode;

REDIS* redis_connect(char *host, char *port);
void   redis_close(REDIS *redis);

RedisCmd*     redisCmd_new(RedisProtocolType protocolType, char *cmdName);
RedisCmd*     redisCmd_newFromStr(RedisProtocolType protocolType,
                                  char *cmdStr,
                                  int cmdLen);
int          redisCmd_addArg(RedisCmd *cmd, char *arg, size_t arglen);
void         redisCmd_free(RedisCmd *cmd);
bstr_t       redisCmd_buildProtocolStr(RedisCmd *cmd);
RedisRetVal* redisCmd_exec(REDIS *redis, RedisCmd *cmd);
int          redisCmd_reset(RedisCmd *cmd, char *cmdName);
int          redisCmd_setProtocolType(RedisCmd *cmd,
                                           RedisProtocolType protocol);
RedisRetVal* redisCmd_getRetVal(RedisCmd *cmd);

void         redisRetVal_free(RedisRetVal *rv);

RedisRetVal* redis_exec(REDIS *redis,
                        RedisProtocolType protocol,
                        char *cmdName,
                        ...)__attribute__ ((sentinel));
RedisRetVal* redis_execStr(REDIS *redis,
                           RedisProtocolType protocol,
                           char *cmdStr,
                           int cmdStrLen);
const char* redisError_getStr(redisErrorCode errorCode);
const char* redisError_getSysErrorStr(redisErrorCode errorCode, int sysErrCode);
#endif /* REDIS_H_ */
