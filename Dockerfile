FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential tini cmake git python3 python3-pip python3-dev pipenv libprotobuf-dev protobuf-compiler openssh-client ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Pre-create known_hosts to avoid interactive prompts
# (do this in the same RUN that needs it, or persist it if you like)
# Use BuildKit's SSH mount to access your host SSH agent during git clone.
RUN --mount=type=ssh bash -euo pipefail <<'EOF'
mkdir -p -m 0700 /root/.ssh
ssh-keyscan -t ed25519 gitlab.hrz.tu-chemnitz.de >> /root/.ssh/known_hosts

# Clone private repos over SSH using the forwarded agent
git clone git@gitlab.hrz.tu-chemnitz.de:ws25-db-sya/ws25-optimizer-rust.git ws25-optimizer-cpp
EOF

WORKDIR /ws25-optimizer-cpp

RUN chmod +x regenerate_proto.sh
RUN ./regenerate_proto.sh

WORKDIR /ws25-optimizer-cpp/cpp

RUN cmake -S . -B build && cmake --build build --config Release

WORKDIR /ws25-optimizer-cpp/python

# Create in-project venv and prep build tooling, then install from lockfile
RUN PIPENV_VENV_IN_PROJECT=1 pipenv run pip install --upgrade pip setuptools wheel && \
    PIPENV_VENV_IN_PROJECT=1 pipenv install --deploy --ignore-pipfile

# Make sure container Python uses Pipenv environment
ENV PATH="/ws25-optimizer-cpp/python/.venv/bin:${PATH}"

# --- Add Star Schema Benchmark data generator (ssb-dbgen) ---
# WORKDIR /opt
# RUN git clone https://github.com/greenlion/ssb-dbgen
# WORKDIR /opt/ssb-dbgen
# RUN make -j
# RUN ./dbgen -s 1 -T a -f -v

# RUN mkdir -p /data/ssb
# RUN mv *.tbl /data/ssb/

WORKDIR /ws25-optimizer-cpp

# --- Make container run-until-stopped, with proper signal handling ---
ENTRYPOINT ["/usr/bin/tini","--"]
CMD ["sleep","infinity"]