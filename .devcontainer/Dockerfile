FROM ubuntu:24.04


# Install packages needed to download LLVM
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
  && apt-get -y install --no-install-recommends \
  curl wget ca-certificates \
  && apt-get clean && rm -rf /var/lib/apt/lists/*

# Download LLVM 18.1.8 source
RUN mkdir -p /tmp/llvm
RUN cd /tmp/llvm && \
  curl -L --progress-bar https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-18.1.8.tar.gz > llvmorg-18.1.8.tar.gz
RUN cd /tmp/llvm && \
  tar -xvf llvmorg-18.1.8.tar.gz && \
  mv llvm-project-llvmorg-18.1.8 llvm && \
  rm -v llvmorg-18.1.8.tar.gz

# Install build packages
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
  && apt-get -y install --no-install-recommends \
  git sudo cmake python3 zlib1g make ninja-build build-essential lld \
  && apt-get clean && rm -rf /var/lib/apt/lists/*

# Build LLVM from source
RUN mkdir /tmp/llvm/llvm/build
RUN cd /tmp/llvm/llvm/build && \
  cmake -DLLVM_ENABLE_PROJECTS="clang;compiler-rt;clang-tools-extra" -DCMAKE_BUILD_TYPE=Release -DLLVM_USE_LINKER=lld -S llvm -G "Ninja" ../llvm
RUN cd /tmp/llvm/llvm/build && \
  ninja install -j$(nproc)

# Install additional packages
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive && \
  apt-get -y install --no-install-recommends \
  unzip vim openssh-client eza bat && \
  apt-get clean && rm -rf /var/lib/apt/lists/*

# Overwrite .bashrc
RUN echo > /root/.bashrc

# Download dotfiles
RUN git clone https://github.com/brenfwd/dotfiles.git /root/dotfiles
RUN echo 'source "/root/dotfiles/bash/load.sh"' >> /root/.bashrc

# Set the default command
CMD ["/bin/bash", "-l"]
