### ETAPA 1: Entorno de compilación para Motor WASM (Estadística Matemática)
FROM emscripten/emsdk:latest AS wasm-build
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY ./wasm ./wasm
WORKDIR /app/wasm
RUN cd src && make
### ETAPA 2: CONSTRUCCIÓN DEL FRONTEND (SVELTE + BUN)
FROM oven/bun:1.2-slim AS svelte-build

# Instalamos curl para bajar los assets de MediaPipe
RUN apt-get update && apt-get install -y curl && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY ./frontend/package.json ./frontend/bun.lockb* ./
RUN bun install --frozen-lockfile
COPY ./frontend ./

# 1. Copiar tus archivos WASM de C++ generados en la etapa anterior
COPY --from=wasm-build /app/wasm/dist/*.wasm ./static/
COPY --from=wasm-build /app/wasm/dist/*.js ./static/

# 2. Descargar Assets de MediaPipe para funcionamiento OFFLINE
# Los colocamos en /static/mediapipe para no mezclarlos con tus otros archivos
RUN mkdir -p ./static/mediapipe && \
    # Modelo de detección facial
    curl -L -o ./static/mediapipe/face_landmarker.task \
    "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task" && \
    # Binarios WASM y Glue Code (Asegúrate de que la versión coincida con tu package.json)
    SET_VERSION="0.10.15" && \
    curl -L -o ./static/mediapipe/vision_wasm_internal.wasm "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@${SET_VERSION}/wasm/vision_wasm_internal.wasm" && \
    curl -L -o ./static/mediapipe/vision_wasm_internal.js "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@${SET_VERSION}/wasm/vision_wasm_internal.js" && \
    curl -L -o ./static/mediapipe/vision_wasm_solution_packed_assets_loader.js "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@${SET_VERSION}/wasm/vision_wasm_solution_packed_assets_loader.js" && \
    curl -L -o ./static/mediapipe/vision_wasm_solution_simd_wasm_bin.wasm "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@${SET_VERSION}/wasm/vision_wasm_solution_simd_wasm_bin.wasm"

RUN bun x svelte-kit sync
RUN bun run build

### ETAPA 3: COMPILACIÓN DEL BACKEND (C++ CROW & POSTGRESQL)
FROM alpine:latest AS build
RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    linux-headers \
    openssl-dev \
    openssl-libs-static \
    zlib-dev \
    zlib-static \
    asio-dev \
    postgresql-dev \
    nlohmann-json
RUN echo "appuser:x:10001:10001:appuser:/:/sbin/nologin" > /etc/passwd_app
RUN git clone --depth 1 https://github.com/CrowCpp/Crow.git /tmp/crow && \
    mkdir -p /usr/local/include && \
    cp -r /tmp/crow/include/* /usr/local/include/ && \
    rm -rf /tmp/crow
RUN git clone https://github.com/Thalhammer/jwt-cpp.git /tmp/jwt && \
    cp -r /tmp/jwt/include/* /usr/local/include/ && \
    rm -rf /tmp/jwt
WORKDIR /app
COPY ./backend/src ./src
RUN cd src && make
COPY --from=svelte-build /app/build ./src/static_assets

### ETAPA 4: IMAGEN DE PRODUCCIÓN FINAL (RUNTIME ULTRA-LIGERO)
FROM scratch AS runtime
COPY --from=build /etc/passwd_app /etc/passwd
COPY --from=build /app/build/app /bin/app
COPY --from=build /app/src/static_assets /static_assets
COPY --from=build /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
USER appuser
EXPOSE 8080
ENTRYPOINT ["/bin/app"]