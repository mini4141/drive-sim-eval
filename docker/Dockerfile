# syntax=docker/dockerfile:1
# Multi-stage: C++ build + ctest -> wheel -> slim runtime (and a dev image with pytest/ruff).

FROM python:3.12-slim-bookworm AS toolchain
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential ninja-build git ca-certificates \
      libeigen3-dev nlohmann-json3-dev libgtest-dev \
    && rm -rf /var/lib/apt/lists/*
RUN pip install --no-cache-dir uv==0.10.3 cmake==3.31.6
WORKDIR /src

# Stage 1: C++ core + GoogleTest via ctest (JUnit XML kept in /out)
FROM toolchain AS core-build
COPY core core
COPY bindings bindings
RUN mkdir -p /out \
 && cmake -S core -B /build/core -G Ninja -DCMAKE_BUILD_TYPE=Release -DSIMCORE_BUILD_TESTS=ON \
 && cmake --build /build/core \
 && ctest --test-dir /build/core --output-on-failure --output-junit /out/ctest-junit.xml

# Stage 2: wheel (scikit-build-core + pybind11)
FROM toolchain AS wheel
COPY core core
COPY bindings bindings
COPY simeval simeval
COPY pyproject.toml uv.lock ./
RUN uv build --wheel --out-dir /dist .

# Python dependency layers (locked)
FROM python:3.12-slim-bookworm AS venv-base
RUN pip install --no-cache-dir uv==0.10.3
ENV UV_PROJECT_ENVIRONMENT=/opt/venv UV_LINK_MODE=copy
WORKDIR /src
COPY pyproject.toml uv.lock ./

FROM venv-base AS venv-runtime
RUN uv sync --frozen --no-install-project --no-dev
COPY --from=wheel /dist /dist
RUN uv pip install --python /opt/venv/bin/python --no-deps /dist/*.whl

FROM venv-base AS venv-dev
RUN uv sync --frozen --no-install-project
COPY --from=wheel /dist /dist
RUN uv pip install --python /opt/venv/bin/python --no-deps /dist/*.whl

# Stage 3: slim runtime (also the AWS Batch image)
FROM python:3.12-slim-bookworm AS runtime
ENV PATH=/opt/venv/bin:$PATH PYTHONUNBUFFERED=1 SIMEVAL_ROOT=/app
COPY --from=venv-runtime /opt/venv /opt/venv
WORKDIR /app
COPY configs configs
COPY tools/policy_weights.json tools/policy_weights.json
ENTRYPOINT ["simeval"]

# Dev image: runtime + pytest/ruff; the repo is bind-mounted over /work
FROM python:3.12-slim-bookworm AS dev
ENV PATH=/opt/venv/bin:$PATH PYTHONUNBUFFERED=1
COPY --from=venv-dev /opt/venv /opt/venv
WORKDIR /work
ENTRYPOINT ["simeval"]
