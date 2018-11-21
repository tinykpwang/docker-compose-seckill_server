#include "src/seckill.grpc.pb.h"
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>
#include <iostream>
#include <memory>
#include <string>
#include "Md5.h"
#include <pthread.h>


#define SALT "salt"
#define Random(x) (rand() % x)
#define NUM_THREADS 500

using grpc::Channel;
using grpc::ClientContext;
using grpc::CreateChannel;
using grpc::InsecureChannelCredentials;
using grpc::Status;
using seckill::SeckillRequest;
using seckill::SeckillResponse;
using seckill::SeckillService;

class SeckillClient {
public:
  SeckillClient(std::shared_ptr<Channel> channel)
      : stub_(SeckillService::NewStub(channel)) {}

  std::string seckill(const std::string &usr_name,const std::string &usr_key) {
    SeckillRequest req;
    req.set_usrname(usr_name);
    req.set_usrkey(usr_key);
    SeckillResponse res;
    ClientContext context;
    Status status = stub_->seckill(&context, req, &res);
    if (status.ok()) {
      return res.result();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

private:
  std::unique_ptr<SeckillService::Stub> stub_;
};

void* onSeckill(void* args)
{
    int i = Random(350);
    std::string prikey = std::to_string (i) + std::to_string (i) + SALT;
    MD5 iMD5;
    iMD5.GenerateMD5((unsigned char *)prikey.c_str(), strlen(prikey.c_str()));
    std::string md5_str(iMD5.ToString());
    
    std::string name = std::to_string(i);
    SeckillClient client(CreateChannel("localhost:50051", InsecureChannelCredentials()));
    std::string resultStr = client.seckill(name,md5_str);
    int resI = atoi(resultStr.c_str());
    if (resI == 1) {
        std::cout << "------抢购成功!!!" <<endl;
    } else {
        std::cout << "抢购失败!!!" <<endl;
    }
//    return ((void *)0);
    
//    pthread_detach (pthread_self());
    return ((void *)0);
}


int main(int argc, char *argv[]) {
//     const char *text = "hello world";
//    MD5 iMD5;
//    iMD5.GenerateMD5((unsigned char *)text, strlen(text) );
//    std::string md5_str(iMD5.ToString());
//    std::cout << "md5 is" <<md5_str << endl;
    
//  SeckillClient client(
//      CreateChannel("localhost:50051", InsecureChannelCredentials()));
//  std::string name;
//  while (std::getline(std::cin, name)) {
//    auto reply = client.seckill(name,"thisiskey");
//    std::cout << "Received: " << reply << std::endl;
//  }
    
    // 定义线程的 id 变量，多个变量使用数组
    pthread_t tids[NUM_THREADS];
    for(int i = 0; i < NUM_THREADS; ++i)
    {
        //参数依次是：创建的线程id，线程参数，调用的函数，传入的函数参数
        int ret = pthread_create(&tids[i], NULL, onSeckill, NULL);
        if (ret != 0)
        {
            cout << "pthread_create error: error_code=" << ret << endl;
        }
    }

    //等各个线程退出后，进程才结束，否则进程强制结束了，线程可能还没反应过来；
    pthread_exit(NULL);
    
  return 0;
}

