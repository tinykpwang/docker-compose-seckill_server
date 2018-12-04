FROM seckill-server-tiny_grpctiny:latest

ADD . /seckill_server

WORKDIR /seckill_server

RUN bazel build src:seckill_server

RUN apt-get install build-essential  

#CMD bazel-bin/src/seckill_server