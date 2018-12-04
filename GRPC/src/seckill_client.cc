#include <iostream>
#include <memory>
#include <string>
#include <pthread.h>

#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <grpc/support/log.h>

#include "src/seckill.grpc.pb.h"
#include "mysqlpool.h"
#include "Md5.h"

#define  MYSQL_ADDRESS "localhost"
#define  MYSQL_USRNAME "root"
#define  MYSQL_PORT 3306
#define  MYSQL_USRPASSWORD "14225117sa"
#define  MYSQL_USEDB "seckill"
#define  MYSQL_MAX_CONNECTCOUNT 3

#define Random(x) (rand() % x)
#define SALT "salt"
#define NUM_THREADS 500

int response_count = 0;
int success_count = 0;
std::mutex mtx;




using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using seckill::SeckillRequest;
using seckill::SeckillResponse;
using seckill::SeckillService;

class SeckillClient 
{
 public:
  explicit SeckillClient(std::shared_ptr<Channel> channel)
      : stub_(SeckillService::NewStub(channel)) {}

  std::string seckill(const std::string &usr_name,const std::string &usr_key) 
  {
    SeckillRequest request;
    request.set_usrname(usr_name);
    request.set_usrkey(usr_key);

    SeckillResponse response;
    ClientContext context;
    CompletionQueue cq;

    Status status;
    std::unique_ptr<ClientAsyncResponseReader<SeckillResponse> > rpc(
        stub_->PrepareAsyncseckill(&context, request, &cq));

    rpc->StartCall();
    rpc->Finish(&response, &status, (void*)1);
    void* got_tag;
    bool ok = false;

    GPR_ASSERT(cq.Next(&got_tag, &ok));


    GPR_ASSERT(got_tag == (void*)1);
    GPR_ASSERT(ok);

    if (status.ok()) 
      return response.result();
    else 
      return "RPC failed";
  }

 private:
  std::unique_ptr<SeckillService::Stub> stub_;
};

bool containsDuplicate(vector<int>& nums) 
{
    sort(nums.begin(), nums.end());
    for (int i = 0; i < nums.size() - 1; i++) 
    {
        // cout << nums.size() << endl;
        if (nums[i] == nums[i + 1]) 
            return true;
    }
    return false;
}

bool checkSeckillInfo()
{
    bool isnormol = true;
    MysqlPool *mysql = MysqlPool::getMysqlPoolObject();
    mysql->setParameter(MYSQL_ADDRESS,MYSQL_USRNAME,MYSQL_USRPASSWORD,MYSQL_USEDB,MYSQL_PORT,NULL,0,MYSQL_MAX_CONNECTCOUNT);
    MYSQL *connection = mysql->getOneConnect();

    //检查抢到的数量
    if (success_count > 50)
    {
        isnormol = false;
        std::cout << "抢到过多商品" << std::endl;
    }

    //检查数据库中商品数量
    int total_count = 0;
    std::string selSql("SELECT g_totalcount FROM goods_info WHERE g_id ='10' ");
    if(mysql_query(connection, selSql.c_str()))
    {
        std::cout << "selSql was Error:" << mysql_error(connection);
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
    if (50 - total_count != success_count)
    {
        isnormol = false;
        std::cout << "商品数量不对" << std::endl;
    }

    //检查订单数量、检查订单是否重复、是否未注册用户抢到商品
    std::map<const std::string,std::vector<const char* > >  resul_map = mysql->executeSql("SELECT usr_name FROM order_info");
    if(resul_map.count("usr_name")>0)
    {
        std::vector<const char*> names = resul_map["usr_name"];
        if (names.size() > 0)
        {
            std::vector<int> namesI;
            for (int i=0; i<names.size(); i++)
                namesI.push_back(std::atoi(names[i]));
            if (names.size() != success_count)
            {
                std::cout << "订单数量不对" << std::endl;
                isnormol = false;
            }

            if (containsDuplicate(namesI))
            {
                std::cout << "存在重复订单" << std::endl;
                isnormol = false;
            }
        }     
    }
    delete mysql;

    return isnormol;
}

void* onSeckill(void* args)
{
    int i = *(int *)args;
    std::string prikey = std::to_string (i) + std::to_string (i) + SALT;
    MD5 iMD5;
    iMD5.GenerateMD5((unsigned char *)prikey.c_str(), strlen(prikey.c_str()));
    std::string md5_str(iMD5.ToString());
    std::string name = std::to_string(i);
    // std::cout << "send username: " << "[" << name << "]" << std::endl;
    SeckillClient SeckillClient(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
    std::string reply = SeckillClient.seckill(name,md5_str); 
    // std::cout << "seckill received: " << reply << std::endl;
    int resI = atoi(reply.c_str());

    mtx.lock();
    if (resI == 1)
        ++ success_count;
    ++ response_count;
    mtx.unlock();

    if (response_count == 500)
    {
        if (checkSeckillInfo())
            std::cout << "秒杀结束，一切正常！一共抢到："<< success_count << "件商品！" << std::endl;
        else
            std::cout << "秒杀结束，出现错误！！！一共抢到："<< success_count << "件商品！" << std::endl;
    }
    // pthread_detach(pthread_self());
    return ((void *)0);
}
void prepareTestData(int testType,int data[])
{
    if (testType == 1)//全是注册用户,注册用户名为1~400
    {
        std::cout << "模拟全是注册用户"<< std::endl;
        for (int i = 1; i < NUM_THREADS + 1; ++i)
            data[i-1] = Random(400);

    }else if(testType == 2)//模拟全部不是注册用户
    {
        std::cout << "模拟全不是注册用户"<< std::endl;
        for (int i = 1; i < NUM_THREADS + 1; ++i)
            data[i-1] = Random(400) + 400;
        
    }else if(testType == 3)//模拟单一注册用户大量请求
    {
        std::cout << "模拟单一注册用户大量请求"<< std::endl;
        for (int i = 1; i < NUM_THREADS + 1; ++i)
            data[i-1] = Random(5);

    }else if(testType == 4)//模拟单一未注册用户大量请求
    {
        std::cout << "模拟单一未注册用户大量请求"<< std::endl;
        for (int i = 1; i < NUM_THREADS + 1; ++i)
            data[i-1] = Random(5) + 400;
            
    }else if(testType == 5)//模拟部分注册用户，部分未注册用户
    {
        std::cout << "模拟部分注册用户，部分未注册用户"<< std::endl;
        for (int i = 1; i < NUM_THREADS + 1; ++i)
            data[i-1] = Random(600);
    }
}


int main(int argc, char** argv) 
{
    int names[NUM_THREADS] = {0};

    if (argc > 0)
    {
        int testType = std::atoi(argv[argc-1]);
        prepareTestData(testType,names);
    }
    
  
    pthread_t tids[NUM_THREADS];
    // int names[NUM_THREADS];

    for(int i = 0; i < NUM_THREADS; ++i)
    {
        //参数依次是：创建的线程id，线程参数，调用的函数，传入的函数参数
        int ret = pthread_create(&tids[i], NULL, onSeckill, &names[i]);
        if (ret != 0)
            std::cout << "pthread_create error: error_code=" << ret << std::endl;
    }
    
    //等各个线程退出后，进程才结束，否则进程强制结束了，线程可能还没反应过来；
    pthread_exit(NULL);
    return 0;
}
