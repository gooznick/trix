FROM python:3

# docker build -t perfetto-ui .

RUN git clone --depth 1 --branch v55.3 \
        https://github.com/google/perfetto.git /opt/perfetto

WORKDIR /opt/perfetto

# Install only the UI build dependencies (Node.js etc.) — no native toolchain needed
RUN tools/install-build-deps --ui

# Build the web UI
RUN ui/build

ENTRYPOINT ["./ui/run-dev-server", "--serve-host", "0.0.0.0"]
