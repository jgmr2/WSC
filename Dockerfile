### ETAPA 1: Entorno de compilación para Motor LaTeX Real
FROM emscripten/emsdk:latest AS wasm-build

# Instalamos dependencias del sistema necesarias para motores tipográficos (Fuentes y Gráficos)
# pkg-config es vital para que emcc encuentre las librerías
RUN apt-get update && apt-get install -y \
    pkg-config \
    libfontconfig1-dev \
    libfreetype6-dev \
    libpng-dev \
    zlib1g-dev \
    python3

WORKDIR /app

# Paso crítico: Clonamos e instalamos libHaru o una versión ligera de un motor TeX
# Aquí usaremos libHaru como motor de salida PDF profesional
RUN git clone https://github.com/libharu/libharu.git && \
    cd libharu && \
    mkdir build && cd build && \
    emcmake cmake .. -DHPDF_SHARED=OFF -DHPDF_STATIC=ON && \
    emmake make install

COPY ./wasm ./wasm
WORKDIR /app/wasm

RUN cd src && make

### ETAPA 2: CONSTRUCCIÓN DEL FRONTEND (SVELTE + BUN)
FROM oven/bun:1.1-alpine AS svelte-build
WORKDIR /app
COPY ./frontend/package.json ./frontend/bun.lockb* ./
RUN bun install --frozen-lockfile
COPY ./frontend ./
# Se integran los binarios WASM generados en la Etapa 1
COPY --from=wasm-build /app/wasm/dist/*.wasm ./static/
COPY --from=wasm-build /app/wasm/dist/*.js ./static/
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
# Se integra el bundle de Svelte generado en la Etapa 2
COPY --from=svelte-build /app/build ./src/static_assets

### ETAPA 4: IMAGEN DE PRODUCCIÓN FINAL (RUNTIME ULTRA-LIGERO)
FROM scratch AS runtime
COPY --from=build /app/build/app /bin/app
COPY --from=build /app/src/static_assets /static_assets
COPY --from=build /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
EXPOSE 8080
ENTRYPOINT ["/bin/app"]