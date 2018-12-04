FROM mysql:5.6

# ARGS
ARG CHANGE_SOURCE=false

# Change Timezone
ARG TIME_ZONE=UTC
ENV TIME_ZONE ${TIME_ZONE}
RUN ln -snf /usr/share/zoneinfo/$TIME_ZONE /etc/localtime && echo $TIME_ZONE > /etc/timezone


RUN  apt-get -o Acquire::Check-Valid-Until=false update && apt-get -y upgrade

# Install Base Components
RUN apt-get install -y --no-install-recommends cron vim curl

#CMD service mysql start