# ETAPA 1: Construcción (Build)
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
COPY ./src ./src

# 4. Compilación
# Nota: Asegúrate de que tu Makefile enlace -lpq -lssl -lcrypto
RUN cd src && make

# ETAPA 2: Runtime (Imagen ultra-ligera)
FROM scratch as runtime

# Copiamos el binario
COPY --from=build /app/build/app /bin/app

# IMPORTANTE para SSL: Copiamos los certificados y las librerías necesarias
# Para que funcione en scratch, tu binario debería estar compilado estáticamente.
# Si no es 100% estático, necesitarás copiar las .so (ver sección de tips abajo)
COPY --from=build /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/

EXPOSE 8080

ENTRYPOINT ["/bin/app"]