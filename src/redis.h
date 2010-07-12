/* redis.h
 *
 * Copyright (C) 2010
 *        Sami Bouafif <sami.bouafif@gmail.com>. All Rights Reserved.
 *
 * This file is part of libredis.
 *
 * libredis library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libredis library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libredis library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
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
 *
 * Structure that holds the result of executing a Redis command. The type of
 * the result can be retrieved with redisRetVal_getType(), and the value with
 * one of the <code>redisRetVal_get*</code> function. These functions must be
 * called according to the value returned from redisRetVal_getType(). For
 * example, if redisRetVal_getType() return %REDIS_RETURN_LINE,
 * redisRetVal_getLine() should be called to retrieve the value.
 *
 * Note that #RedisRetVal is usually linked to a #RedisCmd structure and should
 * not be freed manually (it will be freed when redisCmd_free() is called).
 * However, there is two exceptions to this rule, those returned by redis_exec()
 * and redis_execStr().
 * Those ones should be freed with redisRetVal_free() since they are not associated
 * with any #RedisCmd.
 **/
typedef struct _RedisRetVal RedisRetVal;

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

/**
 * RedisCmdArray:
 *
 * This structure offer the possibility to execute multiple #RedisCmd at once.
 * The main difference with a sequential execution is the way commands are sent
 * the Redis server. In other words, it is an implementation of pipelining.<sbr/>
 * With #RedisCmd, every command is sent individually and the result is
 * retrieved for every command. On the other hand, with #RedisCmdArray, commands
 * are sent at once and all results are retrieved with a single call.
 *
 */

typedef struct _RedisCmdArray RedisCmdArray;

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
  REDIS_ERROR_CMD_INVALIDARGNUM,
  REDIS_ERROR_CMD_INVALID,
  REDIS_ERROR_CMD_UNBALANCEDQ,
  REDIS_ERROR_RET_UNVALID,
  REDIS_ERROR_RET_NOTMULTIBULK
} RedisErrorCode;

REDIS* redis_connect(char *host, char *port);
void   redis_close(REDIS *redis);

RedisCmd*     redisCmd_new(RedisProtocolType protocolType, char *cmdName);
RedisCmd*     redisCmd_newFromStr(RedisProtocolType protocolType,
                                  char              *cmdStr,
                                  int               cmdLen);
RedisErrorCode redisCmd_addArg(RedisCmd *cmd, char *arg, size_t arglen);
RedisErrorCode redisCmd_setArg(RedisCmd *cmd,
                               int      argNum,
                               char     *argVal,
                               size_t   argLen);
void           redisCmd_free(RedisCmd *cmd);
bstr_t         redisCmd_buildProtocolStr(RedisCmd *cmd);
RedisErrorCode redisCmd_reset(RedisCmd *cmd, char *cmdName);
int            redisCmd_setProtocolType(RedisCmd *cmd,
                                      RedisProtocolType protocol);
RedisRetVal*   redisCmd_exec(REDIS *redis, RedisCmd *cmd);
bstr_t         redisCmd_getProtocolStr(RedisCmd *cmd);
RedisRetVal*   redisCmd_getRetVal(RedisCmd *cmd);

RedisCmdArray* redisCmdArray_new();
void           redisCmdArray_free(RedisCmdArray *cmdArray);
RedisErrorCode redisCmdArray_addCmd(RedisCmdArray *cmdArray, RedisCmd *cmd);
bstr_t         redisCmdArray_buildProtocolStr(RedisCmdArray *cmdArray);
bstr_t         redisCmdArray_getProtocolStr(RedisCmdArray *cmdArray);
RedisCmd**     redisCmdArray_getCmds(RedisCmdArray *cmdArray);
int            redisCmdArray_getCmdCount(RedisCmdArray *cmdArray);
RedisRetVal**  redisCmdArray_exec(REDIS *redis, RedisCmdArray *cmdArray);
RedisRetVal**  redisCmdArray_getRetVals(RedisCmdArray *cmdArray);

RedisReturnType redisRetVal_getType(RedisRetVal *rv);
bstr_t          redisRetVal_getError(RedisRetVal *rv);
int             redisRetVal_getInteger(RedisRetVal *rv);
bstr_t          redisRetVal_getLine(RedisRetVal *rv);
bstr_t          redisRetVal_getBulk(RedisRetVal *rv);
bstr_t*         redisRetVal_getMultiBulk(RedisRetVal *rv);
int             redisRetVal_getMultiBulkSize(RedisRetVal *rv);
void            redisRetVal_free(RedisRetVal *rv);

RedisRetVal* redis_exec(REDIS *redis,
                        RedisProtocolType protocol,
                        char *cmdName,
                        ...)__attribute__ ((sentinel));
RedisRetVal* redis_execStr(REDIS *redis,
                           RedisProtocolType protocol,
                           char *cmdStr,
                           int cmdStrLen);
const char* redisError_getStr(RedisErrorCode errorCode);
const char* redisError_getSysErrorStr(RedisErrorCode errorCode, int sysErrCode);
#endif /* REDIS_H_ */
