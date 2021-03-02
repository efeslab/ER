FROM ubuntu:18.04
LABEL maintainer="Gefei Zuo<gefeizuo@umich.edu>"
LABEL version="0.2"
LABEL description="Artifact of PLDI2021, paper 'Reproducing Production Failures with Execution Reconstruction'"
WORKDIR /
COPY install.sh .
ARG DEBIAN_FRONTEND=noninteractive
ENV LLVM_COMPILER=clang
ENV LLVM_COMPILER_PATH=/usr/bin
ENV LLVM_CC_NAME=clang-8
ENV LLVM_CXX_NAME=clang++-8
ENV LLVM_LINK_NAME=llvm-link-8
RUN ./install.sh
CMD /bin/bash
