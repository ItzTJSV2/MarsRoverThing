FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y build-essential gcc g++ make git gdb && \
    apt-get clean

# Set working directory
WORKDIR /app

# Copy your source code into the container
COPY . /app

# Default command (can be overridden)
CMD ["/bin/bash"]
