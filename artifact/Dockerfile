FROM ubuntu:18.04
LABEL maintainer="Gefei Zuo<gefeizuo@umich.edu>"
LABEL version="0.1"
LABEL description="Artifact of PLDI2021, paper 'Reproducing Production Failures with Execution Reconstruction'"
WORKDIR /
COPY install.sh .
ARG DEBIAN_FRONTEND=noninteractive
RUN ./install.sh
CMD /bin/bash
