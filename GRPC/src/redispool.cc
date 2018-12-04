#include "redispool.h"


RedisPool* RedisPool::redispool_object = NULL;
std::mutex RedisPool::objectlock;
std::mutex RedisPool::poollock;

RedisPool::RedisPool() {}

/*
 *配置数据库参数
 */
void RedisPool::setParameter( const char*   redishost,
                              unsigned int  port,
                              const char*   socket,
                              unsigned long client_flag,
                              unsigned int  max_connect ) 
{
  _redishost    = redishost;
  _port         = port;
  _socket       = socket;
  _client_flag  = client_flag;
  MAX_CONNECT   = max_connect;
  connect_count = 0;
}
  
/*
 *有参的单例函数，用于第一次获取连接池对象，初始化数据库信息。
 */
RedisPool* RedisPool::getRedisPoolObject() 
{
  if (redispool_object == NULL) 
  { 
    objectlock.lock();
    if (redispool_object == NULL) 
    {
      redispool_object = new RedisPool();
    }
    objectlock.unlock();
  }
  return redispool_object;
}
                                                 
/*
 *创建一个连接对象
 */
redisContext* RedisPool::createOneConnect() 
{
  redisContext* conn = NULL;
  conn = redisConnect(_redishost, _port);
  if (conn != NULL && conn->err) 
  {
    std::cout << "redis connection error:" << conn->errstr <<std::endl;
    return NULL;
  }
  return conn;
}

/*
 *判断当前redis连接池的是否空
 */
bool RedisPool::isEmpty() 
{
  return redispool.empty();
}
/*
 *获取当前连接池队列的队头
 */
redisContext* RedisPool::poolFront() 
{
  return redispool.front();
}
/*
 *
 */
unsigned int RedisPool::poolSize() 
{
 return redispool.size();
}
/*
 *弹出当前连接池队列的队头
 */
void RedisPool::poolPop() 
{
  redispool.pop();
}

redisContext* RedisPool::getOneConnect() 
{
  poollock.lock();
  redisContext *conn = NULL;
  if (!isEmpty()) 
  {
    if (!isEmpty()) 
    {
      conn = poolFront();
      poolPop();
    } else 
    {
      if (connect_count < MAX_CONNECT)
        conn = createOneConnect(); 
      else 
        std::cerr << "the number of redis connections is too much!" << std::endl;
    }
  } else 
  {
    if (connect_count < MAX_CONNECT)
      conn = createOneConnect(); 
    else 
      std::cerr << "the number of redis connections is too much!" << std::endl;
  }
  poollock.unlock();
  return conn;
}
/*
 *将有效的链接对象放回链接池队列中，以待下次的取用。
 */
void RedisPool::close(redisContext* conn) 
{
  if (conn != NULL) 
  {
    poollock.lock();
    redispool.push(conn);
    poollock.unlock();
  }
}

/*
 * 析构函数，将连接池队列中的连接全部关闭
 */
RedisPool::~RedisPool() 
{
  while (poolSize() != 0) 
  {
    redisFree(poolFront());
    poolPop();
    connect_count--;
  }
}


