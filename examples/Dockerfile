FROM us-docker.pkg.dev/qpoint-edge/public/qcontrol:latest AS qcontrol

FROM debian:bookworm-slim

# Install build essentials and dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    git \
    curl \
    ca-certificates \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && npm install -g @anthropic-ai/claude-code \
    && rm -rf /var/lib/apt/lists/*

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

CMD ["/bin/bash"]
