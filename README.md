# docker-compose-grpc-bazel-mysql-redis

## 用grpc C++实现的秒杀服务,docker-compose管理grpc、redis、mysql,代码编译使用bazel

## 服务端实现原理
> 1.采用grpc服务异步形式,利用grpc的消息队列存储秒杀请求
> 2.利用多个worker线程从消息队列中取出秒杀请求处理
> 3.异步处理秒杀请求,利用redis依次进行身份校验、重复订单校验、扣除订单操作
> 4.redis订单扣除成功则把订单信息录入mysql数据库,录入mysql数据库失败则记录下来重新加入redis

## 容错逻辑
> redis连接失败,则身份校验、重复订单校验、扣除订单操作直接在mysql中进行

## How to use
- 服务端启动方法(需要安装docker、docker-compose):
> git clone https://github.com/tinykpwang/docker-compose-seckill_server
> cd docker-compose-seckill_server  (安装bazel环境的过程可能有点长)
> docker-compose up --build

- 客户端启动方法(本地命令行客户端,需要安装grpc C++、bazel):
> ulimit -n 2000
> bazel build src:seckill_client
> bazel-bin/src/seckill_client

## 怎样验证server行为
1.本地命令行客户端会打印抢购结果日志
2.docker 容器内的服务端也会打印相应结果日志
3.本地docker-compose-seckill_server/components/mysql/data/seckill文件夹中会有相关数据库文件
4.抢购结束后,可以使用 "docker exec -it seckill-server_mysql /bin/bash" 命令进入mysql容器内部,使用"mysql -uroot -p14225117sa"查看相关数据是否正确,  也可以使用"docker exec -it seckill-server_redis /bin/bash" 命令进入redis容器,使用"redis-cli"登陆,查看相关数据信息,验证抢购结果



