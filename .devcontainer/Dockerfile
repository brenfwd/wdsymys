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
RUN ln -s /usr/bin/opt-18 /usr/local/bin/opt

RUN ln -s /usr/include/llvm-18/llvm /usr/include/llvm
RUN ln -s /usr/include/llvm-c-18/llvm-c /usr/include/llvm-c
RUN ln -s /usr/lib/llvm-18 /usr/lib/llvm

# Install additional packages
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive && \
  apt-get -y install --no-install-recommends \
  unzip vim openssh-client eza bat git cmake ninja-build make build-essential && \
  apt-get clean && rm -rf /var/lib/apt/lists/*


ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000

# Add a `vscode` user
RUN deluser ubuntu
RUN groupadd --gid ${USER_GID} ${USERNAME}
RUN useradd --uid ${USER_UID} --gid ${USER_GID} ${USERNAME} -s /bin/bash
RUN cp -r /etc/skel /home/${USERNAME}
RUN chown -R ${USERNAME}:${USERNAME} /home/${USERNAME}

USER ${USERNAME}

# Overwrite .bashrc
RUN echo > /home/${USERNAME}/.bashrc

# Download dotfiles
RUN git clone https://github.com/brenfwd/dotfiles.git /home/${USERNAME}/dotfiles
RUN echo 'source "$HOME/dotfiles/bash/load.sh"' >> /home/${USERNAME}/.bashrc

# Set the default command
CMD ["/bin/bash", "-l"]
