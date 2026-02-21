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
WORKDIR /app

# 1. Instalar dependencias primero (Caché de capas)
COPY ./frontend/package.json ./frontend/bun.lockb* ./
RUN bun install --frozen-lockfile

# 2. Copiar el resto del código del frontend
COPY ./frontend ./

# 3. CAMBIO CLAVE: Copiar modelos locales en lugar de descargarlos
# Asumiendo que están en frontend/static/models en tu host
COPY ./frontend/static/models/ ./static/models/

# 4. Copiar artefactos de WASM a la carpeta static
COPY --from=wasm-build /app/wasm/dist/*.wasm ./static/
COPY --from=wasm-build /app/wasm/dist/*.js ./static/

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

# Instalación de librerías de C++ (Crow y JWT)
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
# Traemos el build de Svelte
COPY --from=svelte-build /app/build ./src/static_assets

### ETAPA 4: IMAGEN DE PRODUCCIÓN FINAL (RUNTIME ULTRA-LIGERO)
FROM scratch AS runtime
COPY --from=build /app/build/app /bin/app
COPY --from=build /app/src/static_assets /static_assets
COPY --from=build /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
EXPOSE 8080
ENTRYPOINT ["/bin/app"]