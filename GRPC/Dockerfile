FROM grpc/cxx:latest

ADD . /seckill_server

WORKDIR /seckill_server

RUN apt-get clean all

RUN apt-get update

RUN apt-get install -y openjdk-8-jdk

RUN echo "deb [arch=amd64] http://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list

RUN curl https://bazel.build/bazel-release.pub.gpg | apt-key add -

RUN apt-get -y update && apt-get install -y bazel

RUN apt-get install -y libmysql++-dev

RUN apt-get install -y mysql-client

RUN apt-get install -y libmysqlcppconn-dev

RUN apt-get install -y libhiredis-dev

RUN bazel build src:seckill_server

CMD bazel-bin/src/seckill_server
