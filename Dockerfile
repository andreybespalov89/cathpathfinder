FROM nvidia/cuda:12.1.1-cudnn8-devel-ubuntu22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
  && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
    git \
    bzip2 \
    xz-utils \
    pkg-config \
  && rm -rf /var/lib/apt/lists/*

ENV MAMBA_PREFIX=/opt/mambaforge
ENV PATH=${MAMBA_PREFIX}/bin:${PATH}
ENV TORCH_HOME=/opt/torch
ENV CUDA_HOME=/usr/local/cuda

WORKDIR /app

COPY deploy /app/deploy
RUN bash /app/deploy/01-install-mambaforge.sh

COPY . /app

ENV ESMFOLD_CUDA=1
ENV CUDA_VERSION=12.1
RUN bash /app/deploy/02-create-envs.sh
RUN bash /app/deploy/03-install-project.sh

RUN mkdir -p /opt/torch/hub/checkpoints
COPY weights/ /opt/torch/hub/checkpoints/

COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["entrypoint.sh"]
