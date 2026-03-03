FROM us-docker.pkg.dev/qpoint-edge/public/qcontrol:latest AS qcontrol

FROM debian:bookworm-slim

# Install build essentials and dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    git \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Claude Code native binary
RUN curl -fsSL https://claude.ai/install.sh | bash

# Ensure Claude is in PATH for all shells
ENV PATH="/root/.local/bin:${PATH}"

# Copy qcontrol binary from upstream image
COPY --from=qcontrol /usr/local/bin/qcontrol /usr/local/bin/qcontrol

# Copy SDK source
WORKDIR /workspace
COPY . .

# Build examples to verify everything works, then clean
RUN cd examples && make clean && make all && make clean

# Add MOTD
COPY examples/motd.txt /etc/motd
RUN echo 'cat /etc/motd' >> /root/.bashrc

# Set default working directory for interactive shell
WORKDIR /workspace/examples

CMD ["/bin/bash"]
