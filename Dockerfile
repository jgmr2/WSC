### ETAPA 1: COMPILACIÓN DE LÓGICA EN C++ A WEBASSEMBLY (WASM)
FROM emscripten/emsdk:latest as wasm-build
WORKDIR /app
COPY ./wasm ./wasm
WORKDIR /app/wasm
RUN cd src && make

### ETAPA 2: CONSTRUCCIÓN DEL FRONTEND (SVELTE + BUN)
FROM oven/bun:1.1-alpine as svelte-build
WORKDIR /app
COPY ./frontend/package.json ./frontend/bun.lockb* ./
RUN bun install --frozen-lockfile
COPY ./frontend ./
# Se integran los binarios WASM generados en la Etapa 1
COPY --from=wasm-build /app/wasm/dist/*.wasm ./static/
COPY --from=wasm-build /app/wasm/dist/*.js ./static/
RUN bun run build

### ETAPA 3: COMPILACIÓN DEL BACKEND (C++ CROW & POSTGRESQL)
FROM alpine:latest as build
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
FROM scratch as runtime
COPY --from=build /app/build/app /bin/app
COPY --from=build /app/src/static_assets /static_assets
COPY --from=build /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
EXPOSE 8080
ENTRYPOINT ["/bin/app"]