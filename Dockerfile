ARG BASE_IMAGE=ubuntu:24.10

FROM ${BASE_IMAGE}
LABEL version="1.0" description="dockerfile for Tdesktop build" author="zengtielong"

USER root
# 配置环境及工作目录
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update -y
RUN apt-get install -y proxychains
RUN sed -i 's/^.*socks4.*127.0.0.1.*9050.*/socks5 172.24.109.16 8080\nhttp 172.24.109.16 808/' /etc/proxychains.conf

RUN apt-get install -y curl git gcc g++ unzip gdb valgrind nano tzdata sudo python3-pip

# 创建一个普通用户
RUN useradd -m compiler

# 将用户添加到 sudo 组
RUN usermod -aG sudo compiler

# 配置 sudo 无需密码
RUN echo "compiler ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

# 切换到普通用户
USER compiler
ENV DEBIAN_FRONTEND=noninteractive

# 配置 git ssh 免密密钥，需要提前在宿主机生成好密钥
ENV MY_HOME /home/compiler
WORKDIR $MY_HOME

# 配置代理
ENV GIT_SSH_COMMAND="ssh -o StrictHostKeyChecking=no"
# ==================== 0.1 安装依赖 ====================
# 安装 xmake
USER compiler
RUN curl -fsSL https://xmake.io/shget.text | proxychains bash
RUN sudo ln -s  ${MY_HOME}/.local/bin/xmake /usr/bin/xmake

# =================== 0.2 安装xmake ====================
# 复制项目主xmake.lua文件，里面应当包含项目的所有依赖。
ENV PROJECT_NAME source
ENV PROJECT_DIR ${MY_HOME}/${PROJECT_NAME}
WORKDIR $MY_HOME

# 设置项目目录挂载点，启动时将宿主机目录挂载到容器中，以使用本地项目进行编译调试。启动容器时需要加上 -v /宿主机项目目录:/${PROJECT_NAME}
VOLUME ["/${PROJECT_NAME}"]
WORKDIR /${PROJECT_NAME}
