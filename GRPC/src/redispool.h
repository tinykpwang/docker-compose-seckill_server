#ifndef REDISPOOL_H
#define REDISPOOL_H

#include<iostream>
#include <hiredis/hiredis.h>
#include<queue>
#include<map>
#include<vector>
#include<utility>
#include<string>
#include<mutex>
#include<thread>



class RedisPool {
  public:
    ~RedisPool();
    static RedisPool* getRedisPoolObject();              //单列模式获取本类的对象
    void setParameter( const char*   _redishost,
                       unsigned int  _port = 0,
                       const char*   _socket = NULL,
                       unsigned long _client_flag = 0,
                       unsigned int  MAX_CONNECT = 50 );              //设置数据库参数
    redisContext* getOneConnect();                       //获取一个连接对象
    void close(redisContext* conn);                      //关闭连接对象
  private:
    RedisPool();
    redisContext* createOneConnect();                    //创建一个新的连接对象
    bool isEmpty();                               //连接池队列池是否为空
    redisContext* poolFront();                           //连接池队列的队头
    unsigned int poolSize();                      //获取连接池的大小
    void poolPop();                               //弹出连接池队列的队头
  private:
    std::queue<redisContext*> redispool;                 //连接池队列
    const char*   _redishost;                     //redis主机地址
    unsigned int  _port;                          //redis端口
    const char*   _socket;                        //可以设置成Socket or Pipeline，通常设置为NULL
    unsigned long _client_flag;                   //设置为0
    unsigned int  MAX_CONNECT;                    //同时允许最大连接对象数量
    unsigned int  connect_count;                  //目前连接池的连接对象数量
    static std::mutex objectlock;                 //对象锁
    static std::mutex poollock;                   //连接池锁
    static RedisPool* redispool_object;           //类的对象
};

#endif
