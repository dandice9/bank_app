from debian:bookworm

RUN apt update -y && apt upgrade -y \
	&& apt install -y build-essential cmake openssl libssl-dev
  
EXPOSE 80