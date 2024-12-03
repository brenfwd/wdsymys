FROM ubuntu:24.04


RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
  && apt-get -y install --no-install-recommends \
  software-properties-common wget

RUN wget -O /tmp/llvm.sh https://apt.llvm.org/llvm.sh && \
  chmod +x /tmp/llvm.sh && \
  /tmp/llvm.sh 18 all

# Add symlinks
RUN bash -c 'for clang_bin in /usr/bin/clang*-18; do [[ -e $clang_bin ]] && ln -sf "$clang_bin" "/usr/local/bin/$(basename "$clang_bin" | sed 's/-18$//')"; done'
RUN bash -c 'for ll_bin in /usr/bin/ll*-18; do [[ -e $ll_bin ]] && ln -sf "$ll_bin" "/usr/local/bin/$(basename "$ll_bin" | sed 's/-18$//')"; done'

RUN ln -s /usr/include/llvm-18/llvm /usr/include/llvm
RUN ln -s /usr/include/llvm-c-18/llvm-c /usr/include/llvm-c

# Install additional packages
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive && \
  apt-get -y install --no-install-recommends \
  unzip vim openssh-client eza bat git cmake ninja-build make build-essential && \
  apt-get clean && rm -rf /var/lib/apt/lists/*

# Overwrite .bashrc
RUN echo > /root/.bashrc

# Download dotfiles
RUN git clone https://github.com/brenfwd/dotfiles.git /root/dotfiles
RUN echo 'source "/root/dotfiles/bash/load.sh"' >> /root/.bashrc

# Set the default command
CMD ["/bin/bash", "-l"]
