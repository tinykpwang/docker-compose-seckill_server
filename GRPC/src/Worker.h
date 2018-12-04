
#ifndef _WORKER_H
#define _WORKER_H


#include <grpc++/security/server_credentials.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc/grpc.h>
#include <grpc/support/log.h>

#include "src/seckill.grpc.pb.h"

#include <hiredis/hiredis.h>

#include "Md5.h"
// #include "Utils.h"
#include "mysqlpool.h"
#include "redispool.h"
#include <thread>
#include <unistd.h>
#include <pthread.h>

#define SALT "salt"

using namespace std;
using grpc::Status;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using seckill::SeckillRequest;
using seckill::SeckillResponse;
using seckill::SeckillService;
using grpc::ServerAsyncResponseWriter;

class Worker
{
public:
    
    Worker(SeckillService::AsyncService* service, ServerCompletionQueue* cq, MysqlPool *mysql, RedisPool *redis,int *failedCount, pthread_rwlock_t *rwlock);

    void Proceed();

private:
    //用户权限校验
    bool checkUserInfo(std::string usr_name,std::string usr_key,redisContext *redisconn,MYSQL *mysqlcon);
    bool checkUserInfo_mysql(std::string usr_name,std::string usr_key,MYSQL *mysqlcon);
    
    //重复性校验
    bool checkUserHasGot(std::string usr_name,std::string usr_key,redisContext *redisconn,MYSQL *mysqlcon);
    bool checkUserHasGot_mysql(std::string usr_name,std::string usr_key,MYSQL *mysqlcon);
    
    //抢购过程
    void seckillGoods(std::string usr_name,std::string usr_key,redisContext *redisconn,MYSQL *connection,int repeatCount);
    void seckillGoods_mysql(std::string usr_name,std::string usr_key,MYSQL *connection);
    
    SeckillService::AsyncService* service_;
    // 消息队列
    ServerCompletionQueue* cq_;

    ServerContext ctx_;

    SeckillRequest request_;

    SeckillResponse response_;

    ServerAsyncResponseWriter<SeckillResponse> responder_;

    enum WorkerStatus { CREATE, PROCESS, FINISH };
    // worker当前状态
    WorkerStatus status_;

    //数据库连接池
    MysqlPool *mysql_;

    //redis连接池
    RedisPool *redis_;

    //失败次数
    int *failedCount_;
    //失败次数的读写锁
    pthread_rwlock_t *rwlock_;
};
#endif

