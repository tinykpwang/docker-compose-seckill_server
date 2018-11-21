#include "src/seckill.grpc.pb.h"
#include <grpc++/security/server_credentials.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc/grpc.h>
#include <iostream>
#include <memory>
#include <string>
#include <mysql/mysql.h>
#include <hiredis/hiredis.h>
#include "Md5.h"
#include <mutex>

#define SALT "salt"
#define RETRYCOUNT 10

bool hasStored = false;
std::mutex mtx;

using seckill::SeckillRequest;
using seckill::SeckillResponse;
using seckill::SeckillService;
using grpc::InsecureServerCredentials;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

MYSQL * GetMySqlConnection(){
    //开启mysql
    MYSQL *connection = mysql_init(NULL);
    std::string host("mymysql");
    std::string user("tinykpwang");
    std::string pwd("14225117sa");
    std::string db_name("seckill");
    connection = mysql_real_connect(connection, host.c_str(),
                                    user.c_str(), pwd.c_str(), db_name.c_str(), 0, NULL, 0);
    if(connection == NULL)
    {
        std::cout << "mysql Error:" << mysql_error(connection);
        exit(1);
    }
    return connection;
}

redisContext *GetRedisConnection(){
    //开启redis
    redisContext *redisconn = redisConnect("myredis", 6379);
    if (redisconn != NULL && redisconn->err) {
        std::cout << "redis connection error:" << redisconn->errstr <<std::endl;
        redisFree(redisconn);
        exit(1);
    }
    return redisconn;
}

class SeckillServiceImpl final : public SeckillService::Service {
public:
  Status seckill(ServerContext *context, const SeckillRequest *request,
              SeckillResponse *response) override {
    auto usr_name = request->usrname();
    auto usr_key = request->usrkey();
      
      //连接redis
      redisContext *redisconn = GetRedisConnection();

      //用户名权限校验
      if (checkUserInfo(usr_name,usr_key,redisconn)) {
          //重复性校验
          if (!usrHasGot(usr_name,usr_key,redisconn)) {
              //开始抢购
              seckillGoods(usr_name,usr_key,redisconn,response,0);
          }else{
              response->set_result("0");
          }
      } else {
//          std::cout << "Seckill failed!!!" <<std::endl;
          response->set_result("0");
      }

    redisFree(redisconn);
      std::cout << "Received:usr_name: " << usr_name << "  usr_key:" << usr_key << std::endl;
    return Status::OK;
  }
  
    bool usrHasGot(std::string usr_name,std::string usr_key,redisContext *redisconn){
        redisReply *orderReply = (redisReply *)redisCommand(redisconn, "HGET order_info %s", usr_name.c_str());
        if (orderReply != NULL && orderReply->type == REDIS_REPLY_STRING ) {
            if (orderReply->str != NULL) {
//                std::cout << "Users have purchased it!!!" <<endl;
                return true;
            }
        }
        freeReplyObject(orderReply);
        return false;
    }

    bool checkUserInfo(std::string usr_name,std::string usr_key,redisContext *redisconn){
        redisReply *confirmReply = (redisReply *)redisCommand(redisconn, "HGET usr_info %s", usr_name.c_str());
        if (confirmReply != NULL && confirmReply->type == REDIS_REPLY_STRING ) {
            std::string key(confirmReply->str);
            if (usr_key == key) {
                return true;
            } else {
//                std::cout << "Password error!!!" <<endl;
            }
        }else{
//            std::cout << "User name does not exist!!!" <<endl;
        }
        freeReplyObject(confirmReply);
        return false;
    }

    void seckillGoods(std::string usr_name,std::string usr_key,redisContext *redisconn,SeckillResponse *response,int repeatCount){
        ++repeatCount;
//        std::cout << "satrt shopping!" << std::endl;
        int currGoods_count = 0;

        redisReply *reply = (redisReply *)redisCommand(redisconn, "GET %s", "total_count");

        redisReply *watchReply = (redisReply *)redisCommand(redisconn, "WATCH %s", "total_count");

        if (reply != NULL && watchReply != NULL && watchReply->type == REDIS_REPLY_STATUS && (strcasecmp(watchReply->str,"OK") == 0)) {
            currGoods_count = atoi(reply->str);

            freeReplyObject(watchReply);
            freeReplyObject(reply);
        }else{
            response->set_result("0");
            return;
        }

        if (currGoods_count > 0) {
            //开启事务
            redisReply *multiReply = NULL;
            multiReply  = (redisReply *)redisCommand(redisconn, "MULTI");
            if (/*watchReply != NULL && watchReply->type == REDIS_REPLY_STATUS && (strcasecmp(watchReply->str,"OK") == 0) && */multiReply != NULL && multiReply->type == REDIS_REPLY_STATUS && (strcasecmp(multiReply->str,"OK") == 0)) {
                redisCommand(redisconn, "DECR %s", "total_count");

                redisReply *execReply = NULL;
                execReply  = (redisReply *)redisCommand(redisconn, "EXEC");
                if (execReply != NULL && execReply->type == REDIS_REPLY_ARRAY && (execReply->elements > 0)) {
                    freeReplyObject(execReply);
                    redisReply *gotReply  = (redisReply *)redisCommand(redisconn, "HMSET order_info  %s %s", usr_name.c_str(), usr_key.c_str());
                    freeReplyObject(gotReply);
                    response->set_result("1");
                    return;
                } else {
                    //失败重试
                    if (execReply != NULL ) {
                        freeReplyObject(execReply);
                    }
//                    std::cout << "reckill failed,retry" << std::endl;
                    redisCommand(redisconn, "UNWATCH");
                    if (repeatCount < RETRYCOUNT) {
                        seckillGoods(usr_name,usr_key,redisconn,response,repeatCount);
                    }else{
                        response->set_result("0");
                    }
                }

            } else {
//                std::cout << "watch or multi failed reckill failed,retry" << std::endl;
                redisCommand(redisconn, "UNWATCH");
                if (repeatCount < RETRYCOUNT) {
                    seckillGoods(usr_name,usr_key,redisconn,response,repeatCount);
                }else{
                    response->set_result("0");
                }
            }
            freeReplyObject(multiReply);
            return;
        } else {
            redisCommand(redisconn, "UNWATCH");
            mtx.lock();
            if (!hasStored) {
                hasStored = true;
                mtx.unlock();

                MYSQL *connection = GetMySqlConnection();

                std::string createSql("CREATE TABLE order_info(usr_name VARCHAR(100) NOT NULL,usr_key VARCHAR(100) NOT NULL,goods_id VARCHAR(100) NOT NULL,PRIMARY KEY (usr_name))");
                mysql_query(connection, createSql.c_str());

                redisReply *insertO = (redisReply *)redisCommand(redisconn, "HKEYS %s", "order_info");
                if (insertO != NULL && insertO->type == REDIS_REPLY_ARRAY ) {

                    std::string insertOSql("INSERT INTO order_info(usr_name,usr_key,goods_id) VALUES ");
                    for (int i = 0; i < insertO->elements; i++) {

                        std::string usrName(insertO->element[i]->str);

                        redisReply *getR = (redisReply *)redisCommand(redisconn, "HMGET %s %s","order_info",usrName.c_str());
                        if (i == insertO->elements - 1) {
                            insertOSql = insertOSql + "('" + usrName + "','" + getR->element[0]->str + "','10')";
                        } else {
                            insertOSql = insertOSql + "('" + usrName + "','" + getR->element[0]->str + "','10'),";
                        }
                        freeReplyObject(getR);
                    }

                    std::cout << "插入订单的mysql语句："<< insertOSql << std::endl;
                    mysql_query(connection, insertOSql.c_str());
                }
                //还需要更新商品数量
                std::string insertGoodsSql("INSERT INTO goods_info(g_name, g_id, g_totalcount) VALUES('BMW', '10', 0) ON DUPLICATE KEY UPDATE g_id = '10', g_totalcount = 0");
                mysql_query(connection, insertGoodsSql.c_str());
                mysql_close(connection);
            }
            mtx.unlock();

            response->set_result("0");
//            std::cout << "seckill failed! The goods has sold out!" << std::endl;
            return;
        }
    }
};





void RunServer() {
    
    MYSQL *connection = GetMySqlConnection();
    redisContext *redisconn = GetRedisConnection();

    int total_count = 0;
    //往数据库中插入测试数据,模拟用户注册信息,存储用名+md5(用户名+密码+盐)
    //插入商品数量假数据、删除老的订单假数据
    std::string creatGoodsTSql("CREATE TABLE IF NOT EXISTS goods_info(g_name VARCHAR(20) NOT NULL,g_id VARCHAR(20) NOT NULL,g_totalcount INT UNSIGNED NOT NULL,PRIMARY KEY (g_id))");
    mysql_query(connection, creatGoodsTSql.c_str());

    std::string insertGoodsSql("INSERT INTO goods_info(g_name, g_id, g_totalcount) VALUES('BMW', '10', 50) ON DUPLICATE KEY UPDATE g_id = '10', g_totalcount = 50");
    mysql_query(connection, insertGoodsSql.c_str());

    std::string deleteOrderSql("DROP TABLE IF EXISTS order_info");
    mysql_query(connection, deleteOrderSql.c_str());

    std::string tableSql("SELECT table_name FROM information_schema.TABLES WHERE table_name ='usr_info'");
    if(mysql_query(connection, tableSql.c_str()))
    {
        std::cout << "Query TableInfo Error:" << mysql_error(connection);
        exit(1);
    }
    else
    {
        MYSQL_RES *result = mysql_use_result(connection);
        MYSQL_ROW row;

        //需要插入测试数据
        if ((row = mysql_fetch_row(result)) == NULL) {

            std::string createSql("CREATE TABLE usr_info(usr_name VARCHAR(100) NOT NULL,usr_key VARCHAR(100) NOT NULL,PRIMARY KEY (usr_name))");
            mysql_query(connection, createSql.c_str());

            std::string insertSql("INSERT INTO usr_info(usr_name,usr_key) VALUES ");
            //模拟用户名密码都是i
            for (int i = 1; i <=300; i ++) {
                std::string prikey = std::to_string (i) + std::to_string (i) + SALT;
                MD5 iMD5;
                iMD5.GenerateMD5((unsigned char *)prikey.c_str(), strlen(prikey.c_str()));
                std::string md5_str(iMD5.ToString());

                if (i == 300) {
                    insertSql =  insertSql + "('" + std::to_string (i) + "','" + md5_str + "')";
                } else {
                    insertSql =  insertSql + "('" + std::to_string (i) + "','" + md5_str + "'),";
                }
            }

            std::cout << "插入的sql为" << insertSql << endl;
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
                if (mysql_num_fields(searchResult) > 1) {
                    std::string name(searchrow[0]);
                    std::string key(searchrow[1]);

                    redisReply *reply = NULL;
                    reply = (redisReply *)redisCommand(redisconn, "HMSET usr_info  %s %s", name.c_str(), key.c_str());

                    if(reply != NULL && reply->type == REDIS_REPLY_STATUS && (strcasecmp(reply->str,"OK") == 0))
                    {
                        std::cout << "redis set Usr_Info OK!" << std::endl;
                        freeReplyObject(reply);
                    }else{
                        std::cout << "redis set count error ";
                        if (reply != NULL ) {
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
            if (mysql_num_fields(result) > 1) {
                total_count = atoi(row[2]);
            }
        }
        // 释放结果集的内存
        mysql_free_result(result);
    }


    mysql_close(connection);




    redisReply *reply = NULL;
    std::string scount = std::to_string(total_count);
    reply = (redisReply *)redisCommand(redisconn, "SET %s %s", "total_count", scount.c_str());
    if(reply != NULL && reply->type == REDIS_REPLY_STATUS && (strcasecmp(reply->str,"OK") == 0))
    {
        std::cout << "redis set count OK!" << std::endl;
        freeReplyObject(reply);
    }else{
        std::cout << "redis set count error ";
        if (reply != NULL ) {
            std::cout << "error message:" << redisconn->errstr << std::endl;
            freeReplyObject(reply);
        }
        redisFree(redisconn);
        return;
    }




    reply = (redisReply *)redisCommand(redisconn, "GET %s", "total_count");
    printf("%s\n", reply->str);
    freeReplyObject(reply);

    redisCommand(redisconn, "DEL %s", "order_info");

    redisFree(redisconn);
    
  //初始化server
    std::string address("0.0.0.0:50051");
    SeckillServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(address, InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << address << std::endl;
    server->Wait();
}

int main(int argc, char *argv[]) {
  RunServer();

  return 0;
}
