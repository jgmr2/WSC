# --- ETAPA 1: WASM (C++ a WebAssembly) ---
FROM emscripten/emsdk:latest as wasm-build
WORKDIR /app
# Copiamos solo lo necesario para compilar WASM
COPY ./wasm ./wasm
WORKDIR /app/wasm
# Asumiendo que tu Makefile genera archivos en un folder 'dist' o similar
RUN cd src && make

# --- ETAPA 2: SVELTE (Frontend) ---
FROM node:20-alpine as svelte-build
WORKDIR /app
# 1. Cache de dependencias
COPY ./frontend/package*.json ./
RUN npm install

# 2. Traemos el código de Svelte
COPY ./frontend ./
# 3. TRAEMOS LOS BINARIOS WASM (Paso clave)
# Ajusta 'build/output.wasm' según lo que genere tu Makefile de la ETAPA 1
# Los movemos a 'static' o 'public' para que Svelte los incluya en el bundle
COPY --from=wasm-build /app/wasm/dist/*.wasm ./static/
COPY --from=wasm-build /app/wasm/dist/*.js ./static/

RUN npm run build

# --- ETAPA 3: BACKEND (Crow C++ & PostgreSQL) ---
FROM alpine:latest as build

# 1. Instalamos dependencias necesarias para C++, PostgreSQL, SSL y JWT
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

# 2. Instalamos Crow Headers
RUN git clone --depth 1 https://github.com/CrowCpp/Crow.git /tmp/crow && \
    mkdir -p /usr/local/include && \
    cp -r /tmp/crow/include/* /usr/local/include/ && \
    rm -rf /tmp/crow

# 3. (Opcional) Si usas una librería específica para JWT como Thalhammer/jwt-cpp
RUN git clone https://github.com/Thalhammer/jwt-cpp.git /tmp/jwt && \
    cp -r /tmp/jwt/include/* /usr/local/include/ && \
    rm -rf /tmp/jwt

WORKDIR /app
COPY ./backend/src ./src

# 4. Compilación
# Nota: Asegúrate de que tu Makefile enlace -lpq -lssl -lcrypto
RUN cd src && make

# 4. TRAEMOS EL FRONTEND COMPILADO (Paso clave)
# Svelte suele generar todo en una carpeta 'dist' o 'build'
# La copiamos dentro de la carpeta del backend para que el binario la sirva
COPY --from=svelte-build /app/build ./src/static_assets


# --- ETAPA 4: RUNTIME (Imagen ultra-ligera) ---
FROM scratch as runtime

# Copiamos el binario final (ajusta la ruta según tu Makefile)
COPY --from=build /app/build/app /bin/app

# Copiamos la carpeta de archivos estáticos (Svelte + WASM) 
# Si tu binario de Crow los busca en una ruta relativa al ejecutarse
COPY --from=build /app/src/static_assets /static_assets

# Certificados SSL para que las peticiones HTTPS externas funcionen
COPY --from=build /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/

EXPOSE 8080

ENTRYPOINT ["/bin/app"]