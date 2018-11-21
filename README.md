# docker-compose-grpc-bazel-mysql-redis

## 用grpc C++实现的秒杀服务,docker-compose管理grpc、redis、mysql,代码编译使用bazel

## How to use
- 服务端启动方法(需要安装docker、docker-compose):
> git clone https://github.com/tinykpwang/docker-compose-seckill_server
> cd docker-compose-seckill_server
> docker-compose up

- 客户端启动方法(本地命令行客户端,需要安装grpc C++、bazel):
> git clone https://github.com/tinykpwang/grpc
> cd grpc
> ulimit -n 2000
> bazel build src:seckill_client
> bazel-bin/src/seckill_client

## 怎样验证server行为
1.本地命令行客户端会打印抢购结果日志
2.docker 容器内的服务端也会打印相应结果日志
3.本地docker-compose-seckill_server/components/mysql/data/seckill文件夹中会有相关数据库文件
4.抢购结束后,可以使用 "docker exec -it seckill-server_mysql /bin/bash" 命令进入mysql容器内部,使用"mysql -uroot -p14225117sa"查看相关数据是否正确,  也可以使用"docker exec -it seckill-server_redis /bin/bash" 命令进入redis容器,使用"redis-cli"登陆,查看相关数据信息,验证抢购结果



