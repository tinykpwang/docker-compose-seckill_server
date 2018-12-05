
#include <memory>
#include <iostream>
#include <string>
// #include <thread>
#include "Worker.h"
// #include <pthread.h>
// #include <thread>
// #include <process.h>



#define  SERVER_ADDRESS "0.0.0.0:50051"
#define  MAX_RECEIVE_SIZE 5000
#define  MAX_SEND_SIZE 5000
#define  WORKER_NUM 20

#define  MYSQL_ADDRESS "mymysql"
#define  MYSQL_USRNAME "tinykpwang"
#define  MYSQL_PORT 3306
#define  MYSQL_USRPASSWORD "14225117sa"
#define  MYSQL_USEDB "seckill"
#define  MYSQL_MAX_CONNECTCOUNT 30

#define  REDIS_ADDRESS "myredis"
#define  REDIS_PORT 6379
#define  REDIS_MAX_CONNECTCOUNT 500



using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using seckill::SeckillRequest;
using seckill::SeckillResponse;
using seckill::SeckillService;

class ServerImpl final 
{
 public:
    ~ServerImpl() 
    {
        server_->Shutdown();
        cq_->Shutdown();
    }


    void Run(MysqlPool *mysql,RedisPool *redis) 
    {
        std::string server_address(SERVER_ADDRESS);

        ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service_);
        builder.SetMaxReceiveMessageSize(MAX_RECEIVE_SIZE);
        builder.SetMaxSendMessageSize(MAX_SEND_SIZE);

        //消息队列
        cq_ = builder.AddCompletionQueue();

        server_ = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;
         
        //入库失败的次数以及读写锁
        int failedCounter = 0;
        pthread_rwlock_t rwlock;
        pthread_rwlock_init(&rwlock, NULL);
        
        new Worker(&service_, cq_.get(), mysql,redis,&failedCounter,&rwlock);
        //开启多线程处理rpc调用请求
        pthread_t tids[WORKER_NUM];
        for ( int i = 0 ; i < WORKER_NUM; i++ )
        {
          int ret = pthread_create(&tids[i], NULL, ServerImpl::ThreadHandlerRPC, (void*)this);
          if (ret != 0)
              std::cout << "pthread_create error: error_code=" << ret << std::endl;
        }
        //等各个线程退出后，进程才结束

        for ( int i = 0 ; i < WORKER_NUM; i++ )
        {
            pthread_join(tids[0],NULL);
          
        }
    }
    
    static void* ThreadHandlerRPC(void* lparam) 
    {
        ServerImpl* impl = (ServerImpl*)lparam;
        impl->HandleRPCS();
        return ((void *)0);
    }

    void HandleRPCS() 
    {
        void* tag; 
        bool ok;
        while (true) 
        {
            GPR_ASSERT(cq_->Next(&tag, &ok));
            GPR_ASSERT(ok);
            static_cast<Worker*>(tag)->Proceed();
        }
    }

    std::unique_ptr<ServerCompletionQueue> cq_;
    SeckillService::AsyncService service_;
    std::unique_ptr<Server> server_;
};

void prepareForSeckill(MysqlPool *mymysql,RedisPool *redis){
    MYSQL *connection = mymysql->getOneConnect();
    redisContext *redisconn = redis->getOneConnect();
    
    int total_count = 0;

    //删除老的订单假数据
    std::string creatGoodsTSql("CREATE TABLE IF NOT EXISTS goods_info(g_name VARCHAR(20) NOT NULL,g_id VARCHAR(20) NOT NULL,g_totalcount INT UNSIGNED NOT NULL,PRIMARY KEY (g_id))");
    mysql_query(connection, creatGoodsTSql.c_str());

    
    std::string insertGoodsSql("INSERT INTO goods_info(g_name, g_id, g_totalcount) VALUES('BMW', '10', 50) ON DUPLICATE KEY UPDATE g_id = '10', g_totalcount = 50");
    mysql_query(connection, insertGoodsSql.c_str());

    
    std::string deleteOrderSql("DROP TABLE IF EXISTS order_info");
    mysql_query(connection, deleteOrderSql.c_str());
    
    std::string tableSql("SELECT table_name FROM information_schema.TABLES WHERE table_name ='usr_info'");
    if(mysql_query(connection, tableSql.c_str()))
    // if(mysql_query(connection, tesetsql.c_str()))
    {
        std::cout << "Query TableInfo Error:" << mysql_error(connection);
        exit(1);
    }
    else
    {
        MYSQL_RES *result = mysql_use_result(connection);
        MYSQL_ROW row;

        //往数据库中插入测试数据,模拟用户注册信息,存储用名+md5(用户名+密码+盐)
        if ((row = mysql_fetch_row(result)) == NULL) 
        {

            std::string createSql("CREATE TABLE usr_info(usr_name VARCHAR(100) NOT NULL,usr_key VARCHAR(100) NOT NULL,PRIMARY KEY (usr_name))");
            mysql_query(connection, createSql.c_str());
            
            std::string insertSql("INSERT INTO usr_info(usr_name,usr_key) VALUES ");
            //模拟用户名密码都是i
            for (int i = 1; i <=400; i ++) 
            {
                std::string prikey = std::to_string (i) + std::to_string (i) + SALT;
                MD5 iMD5;
                iMD5.GenerateMD5((unsigned char *)prikey.c_str(), strlen(prikey.c_str()));
                std::string md5_str(iMD5.ToString());
                
                if (i == 300) 
                    insertSql =  insertSql + "('" + std::to_string (i) + "','" + md5_str + "')";
                else 
                    insertSql =  insertSql + "('" + std::to_string (i) + "','" + md5_str + "'),";
            }
            
            std::cout << "插入的sql为" << insertSql << std::endl;
            mysql_query(connection, insertSql.c_str());
        }
        mysql_free_result(result);
        
        
        //查询数据库,将用户信息load到redis
        std::string searchSql("SELECT * FROM usr_info");

        if(mysql_query(connection, searchSql.c_str()))
        {
            std::cout << "Query Usr_Info Error:" << mysql_error(connection);
            exit(1);
        }
        else
        {
            MYSQL_RES *searchResult = mysql_use_result(connection);
            MYSQL_ROW searchrow;
            while((searchrow = mysql_fetch_row(searchResult)) != NULL)
            {
                if (mysql_num_fields(searchResult) > 1) 
                {
                    std::string name(searchrow[0]);
                    std::string key(searchrow[1]);
                    
                    redisReply *reply = NULL;
                    reply = (redisReply *)redisCommand(redisconn, "HMSET usr_info  %s %s", name.c_str(), key.c_str());

                    if(reply != NULL && reply->type == REDIS_REPLY_STATUS && (strcasecmp(reply->str,"OK") == 0))
                    {
                        // std::cout << "redis set Usr_Info OK!" << std::endl;
                        freeReplyObject(reply);
                    }else
                    {
                        std::cout << "redis set count error ";
                        if (reply != NULL ) 
                        {
                            std::cout << "error message:" << redisconn->errstr << std::endl;
                            freeReplyObject(reply);
                        }
                        redisFree(redisconn);
                        return;
                    }
                }
            }
            
            mysql_free_result(searchResult);
            
        } 
    }
    
    //查询商品数量
    std::string sql("SELECT * FROM goods_info");
    if(mysql_query(connection, sql.c_str()))
    {
        std::cout << "Query GoodsCount Error:" << mysql_error(connection);
        exit(1);
    }
    else
    {
        MYSQL_RES *result = mysql_use_result(connection); // 获取结果集
        MYSQL_ROW row;
        while((row = mysql_fetch_row(result)) != NULL)
        {
            if (mysql_num_fields(result) > 1) 
                total_count = atoi(row[2]);
        }
        // 释放结果集的内存
        mysql_free_result(result);
    }

    std::string createTSql("CREATE TABLE IF NOT EXISTS order_info(usr_name VARCHAR(100) NOT NULL,usr_key VARCHAR(100) NOT NULL,goods_id VARCHAR(100) NOT NULL,PRIMARY KEY (usr_name))");
    mysql_query(connection, createTSql.c_str());
    

    redisReply *reply = NULL;
    std::string scount = std::to_string(total_count);
    reply = (redisReply *)redisCommand(redisconn, "SET %s %s", "total_count", scount.c_str());
    if(reply != NULL && reply->type == REDIS_REPLY_STATUS && (strcasecmp(reply->str,"OK") == 0))
    {
        std::cout << "redis set count OK!" << std::endl;
        freeReplyObject(reply);
    }else
    {
        std::cout << "redis set count error ";
        if (reply != NULL ) 
        {
            std::cout << "error message:" << redisconn->errstr << std::endl;
            freeReplyObject(reply);
        }
        redis->close(redisconn);
        mymysql->close(connection);
        return;
    }

    reply = (redisReply *)redisCommand(redisconn, "GET %s", "total_count");
    freeReplyObject(reply);
    redisCommand(redisconn, "DEL %s", "order_info");

    mymysql->close(connection);
    redis->close(redisconn);
    
}


int main(int argc, char** argv) 
{
  
    //初始化数据库
    MysqlPool *mysql = MysqlPool::getMysqlPoolObject();
    mysql->setParameter(MYSQL_ADDRESS,MYSQL_USRNAME,MYSQL_USRPASSWORD,MYSQL_USEDB,MYSQL_PORT,NULL,0,MYSQL_MAX_CONNECTCOUNT);

    //初始化redis
    RedisPool *redis = RedisPool::getRedisPoolObject();
    redis->setParameter(REDIS_ADDRESS,REDIS_PORT,NULL,0,REDIS_MAX_CONNECTCOUNT);

    //准备初始数据
    prepareForSeckill(mysql,redis);
    ServerImpl server;
    server.Run(mysql,redis);
  
    delete mysql;
    delete redis;

    return 0;
}
