#include "Worker.h"
#include <thread>
// #include <mutex>

#define RETRYCOUNT 20

Worker::Worker(SeckillService::AsyncService* service, ServerCompletionQueue* cq, MysqlPool *mysql,RedisPool *redis, int *failedCount, pthread_rwlock_t *rwlock)
        : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE), mysql_(mysql),redis_(redis),failedCount_(failedCount),rwlock_(rwlock) 
{
    Proceed();
}

void Worker::Proceed() 
{
    if (status_ == CREATE) 
    { 
      status_ = PROCESS;
      service_->Requestseckill(&ctx_, &request_, &responder_, cq_, cq_,this);
    } else if (status_ == PROCESS) 
    {

      new Worker(service_, cq_, mysql_,redis_,failedCount_,rwlock_);

      auto usr_name = request_.usrname();
      auto usr_key = request_.usrkey();
      
      std::cout<< "接收到:"<< usr_name <<std::endl;
      //连接redis
      redisContext *redisconn = redis_->getOneConnect();
      MYSQL *mysqlconnection = mysql_->getOneConnect(); 
      
      //用户名权限校验
      if (checkUserInfo(usr_name,usr_key,redisconn,mysqlconnection)) 
      {
          //重复性校验
          if (!checkUserHasGot(usr_name,usr_key,redisconn,mysqlconnection)) 
          {
              //开始抢购
              seckillGoods(usr_name,usr_key,redisconn,mysqlconnection,0);
          }else
          {
              std::cout << "Seckill failed!!! usr has got" <<std::endl;
              response_.set_result("0");
          }
      } else 
      {
          std::cout << "Seckill failed!!! usr has noprimit" <<std::endl;
          response_.set_result("0");
      }

      redis_->close(redisconn);
      mysql_->close(mysqlconnection);

      status_ = FINISH;
      responder_.Finish(response_, Status::OK, this);
    } else 
    {
      GPR_ASSERT(status_ == FINISH);
      delete this;
    }
}

bool Worker::checkUserInfo(std::string usr_name,std::string usr_key,redisContext *redisconn,MYSQL *mysqlcon)
{
    if (redisconn == NULL)
      return checkUserInfo_mysql(usr_name,usr_key,mysqlcon);

    redisReply *confirmReply = (redisReply *)redisCommand(redisconn, "HGET usr_info %s", usr_name.c_str());
    if (confirmReply != NULL && confirmReply->type == REDIS_REPLY_STRING ) 
    {
      std::string key(confirmReply->str);
      if (usr_key == key) 
        return true;
    }
    freeReplyObject(confirmReply);
    return false;
}

bool Worker::checkUserInfo_mysql(std::string usr_name,std::string usr_key,MYSQL *mysqlcon)
{
    bool isUsr = false;
    char selSql[200] = {'\0'};
    sprintf(selSql, "SELECT usr_key FROM usr_info WHERE usr_name ='%s'", usr_name.c_str());
    // std::cout<< selSql <<std::endl;
    if(mysql_query(mysqlcon, selSql))
    {
        std::cout << "selSql usr_key was Error:" << mysql_error(mysqlcon);
        return false;
    }
    else
    {
        MYSQL_RES *result = mysql_use_result(mysqlcon);
        MYSQL_ROW row;
        while((row = mysql_fetch_row(result)) != NULL)
        {
            if (mysql_num_fields(result) > 0) 
            {
              std::string seusr_key(row[0]);
              if (seusr_key == usr_key)
                isUsr = true;
            }
        }
        if (result != NULL)
          mysql_free_result(result);
    }
    return isUsr;
}


bool Worker::checkUserHasGot(std::string usr_name,std::string usr_key,redisContext *redisconn,MYSQL *mysqlcon)
{
    if (redisconn == NULL)
      return checkUserHasGot_mysql(usr_name,usr_key,mysqlcon);

    redisReply *orderReply = (redisReply *)redisCommand(redisconn, "HGET order_info %s", usr_name.c_str());
    if (orderReply != NULL && orderReply->type == REDIS_REPLY_STRING ) 
    {
      if (orderReply->str != NULL) 
          return true;
    }
    freeReplyObject(orderReply);
    return false;
}

bool Worker::checkUserHasGot_mysql(std::string usr_name,std::string usr_key,MYSQL *mysqlcon)
{
    bool hasgot = false;
    int ON = 1;
    int OFF = 0;
    mysql_autocommit(mysqlcon,OFF);


    char selSql[200] = {'\0'};
    sprintf(selSql, "SELECT * FROM order_info WHERE usr_name ='%s' FOR UPDATE", usr_name.c_str());
    std::string selUsr_key;
    
    if(mysql_query(mysqlcon, selSql))
    {
        std::cout << "selSql usr_key was Error:" << mysql_error(mysqlcon);
        hasgot = false;
    }
    else
    {
        MYSQL_RES *result = mysql_use_result(mysqlcon); // 获取结果集
        MYSQL_ROW row;
        while((row = mysql_fetch_row(result)) != NULL)
        {
          if (mysql_num_fields(result) > 0) 
            hasgot = true;
        }
        if (result != NULL)
          mysql_free_result(result);
    }
    mysql_autocommit(mysqlcon,ON);

    return hasgot;
}


void Worker::seckillGoods(std::string usr_name,std::string usr_key,redisContext *redisconn,MYSQL *connection,int repeatCount)
{
    if (redisconn == NULL)
    {
      seckillGoods_mysql(usr_name,usr_key,connection);
      return;
    }

    ++repeatCount;
    std::cout << "satrt shopping!" << std::endl;
    int currGoods_count = 0;
    
    redisReply *watchReply = (redisReply *)redisCommand(redisconn, "WATCH %s", "total_count");

    redisReply *reply = (redisReply *)redisCommand(redisconn, "GET %s", "total_count");
    
    if (reply != NULL && watchReply != NULL && watchReply->type == REDIS_REPLY_STATUS && (strcasecmp(watchReply->str,"OK") == 0)) 
    {
        currGoods_count = atoi(reply->str);
        
        freeReplyObject(watchReply);
        freeReplyObject(reply);
    }else
    {
        freeReplyObject(watchReply);
        freeReplyObject(reply);
        response_.set_result("0");
        return;
    }
    
    if (currGoods_count > 0) 
    {
        //开启事务
        redisReply *multiReply = NULL;
        multiReply  = (redisReply *)redisCommand(redisconn, "MULTI");
        if (multiReply != NULL && multiReply->type == REDIS_REPLY_STATUS && (strcasecmp(multiReply->str,"OK") == 0)) 
        {
            
            //订单操作
            redisCommand(redisconn, "DECR %s", "total_count");
            redisCommand(redisconn, "HMSET order_info  %s %s", usr_name.c_str(), usr_key.c_str());
            
            redisReply *execReply = NULL;
            execReply  = (redisReply *)redisCommand(redisconn, "EXEC");
            if (execReply != NULL && execReply->type == REDIS_REPLY_ARRAY && (execReply->elements > 1)) 
            {
                
              freeReplyObject(execReply);
              seckillGoods_mysql(usr_name,usr_key,connection);
              return;
            }else 
            {
              //失败重试
              std::cout << "retry!!!" <<std::endl;
              if (execReply != NULL ) 
                  freeReplyObject(execReply);

              redisCommand(redisconn, "UNWATCH");
              if (repeatCount < RETRYCOUNT) 
                  seckillGoods(usr_name,usr_key,redisconn,connection,repeatCount);
              else
                  response_.set_result("0");
            }
            
        }else 
        {
            redisCommand(redisconn, "UNWATCH");
            if (repeatCount < RETRYCOUNT) 
                seckillGoods(usr_name,usr_key,redisconn,connection,repeatCount);
            else
                response_.set_result("0");
        }
        freeReplyObject(multiReply);
        return;
    }else 
    {
        redisCommand(redisconn, "UNWATCH");

        pthread_rwlock_wrlock(rwlock_);
        if (*failedCount_ > 0)
        {
          std::string failedC = std::to_string(*failedCount_);
          redisReply *setCreply = (redisReply *)redisCommand(redisconn, "SET total_count %s", failedC.c_str());
          if (setCreply != NULL &&  setCreply->type == REDIS_REPLY_STATUS && (strcasecmp(setCreply->str,"OK") == 0))
            *failedCount_ = 0;
        }
        pthread_rwlock_unlock(rwlock_);
        
        response_.set_result("0");
        std::cout << "seckill failed! The goods has sold out!" << std::endl;
        return;
    }
}

void Worker::seckillGoods_mysql(std::string usr_name,std::string usr_key,MYSQL *connection)
{
    bool isSuc = true;
    int ON = 1;
    int OFF = 0;

    //开启事务
    mysql_autocommit(connection,OFF); 

    //查询商品数量
    int total_count = 0;
    std::string selSql("SELECT g_totalcount FROM goods_info WHERE g_id ='10' FOR UPDATE");
    if(mysql_query(connection, selSql.c_str()))
    {
        std::cout << "selSql was Error:" << mysql_error(connection);
        isSuc = false;
    }
    else
    {
        MYSQL_RES *result = mysql_use_result(connection);
        MYSQL_ROW row;
        while((row = mysql_fetch_row(result)) != NULL)
        {
            if (mysql_num_fields(result) > 0) 
                total_count = atoi(row[0]);
        }
        if (result != NULL)
          mysql_free_result(result);
    }

    if (total_count > 0)
    {
      -- total_count;
    }else
    {
      response_.set_result("0");
      mysql_autocommit(connection,ON);
      return;
    }

    //修改商品数量
    char updateSql[200] = {'\0'};
    sprintf(updateSql, "UPDATE goods_info SET g_totalcount = %d WHERE g_id = '10' ", total_count);
    if(mysql_query(connection,updateSql))
    {
      std::cout << "updateSql was error:"<< updateSql << std::endl;
      isSuc = false;
    }

    //订单入库
    char insertOSql[200] = {'\0'};
    sprintf(insertOSql, "INSERT INTO order_info(usr_name,usr_key,goods_id) VALUES ('%s','%s','10')", usr_name.c_str(),usr_key.c_str());
    if(mysql_query(connection,insertOSql))
    {
      std::cout << "insertOSql was error" << std::endl;
      isSuc = false;
    }


    if(!isSuc)
    {
      mysql_rollback(connection);
      pthread_rwlock_wrlock(rwlock_);
      ++ *failedCount_;
      pthread_rwlock_unlock(rwlock_);

      response_.set_result("0");
    }
    else
    {
      mysql_commit(connection); 
      response_.set_result("1"); 
      std::cout << "Seckill Succeed!!!" <<std::endl; 
    }

    mysql_autocommit(connection,ON);
}

