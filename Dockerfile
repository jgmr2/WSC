# --- ETAPA 1: Compilación Nginx Mínimo con Módulos Custom ---
FROM alpine:3.19 AS nginx-builder

# Instalación de dependencias de compilación
RUN apk add --no-cache build-base perl linux-headers pcre-dev zlib-dev zlib-static openssl-dev openssl-libs-static binutils

WORKDIR /src
# Copiamos tanto el core de nginx como tu carpeta de módulos
COPY nginx-1.26.0/ ./nginx-1.26.0/
# Asumiendo que 'custom' está dentro de la carpeta que copiaste o la copias aparte:
# COPY custom/ ./nginx-1.26.0/src/http/modules/custom/

WORKDIR /src/nginx-1.26.0

# Ejecución de la configuración y compilación
# Nota: --add-module apunta a la ruta relativa desde la raíz de nginx-1.26.0
RUN ./configure \
    --add-module=src/http/modules/custom \
    --prefix=/etc/nginx \
    --sbin-path=/usr/sbin/nginx \
    --conf-path=/etc/nginx/nginx.conf \
    --pid-path=/tmp/nginx.pid \
    --with-http_ssl_module \
    --with-threads \
    --without-http_ssi_module \
    --without-http_userid_module \
    --without-http_mirror_module \
    --without-http_autoindex_module \
    --without-http_geo_module \
    --without-http_map_module \
    --without-http_split_clients_module \
    --without-http_fastcgi_module \
    --without-http_uwsgi_module \
    --without-http_scgi_module \
    --without-http_grpc_module \
    --without-http_memcached_module \
    --without-http_browser_module \
    --with-cc-opt="-Os -fno-stack-protector" \
    --with-ld-opt="-static -s" && \
    make -j$(nproc) && make install
    
# Setup the rootfs for the final scratch image
RUN mkdir -p /rootfs/tmp /rootfs/etc/nginx/logs /rootfs/var/log/nginx /rootfs/etc
RUN ln -sf /dev/stdout /rootfs/etc/nginx/logs/access.log && \
    ln -sf /dev/stderr /rootfs/etc/nginx/logs/error.log

# Ensure both passwd AND group are created inside /rootfs/etc
RUN echo "root:x:0:0:root:/root:/bin/sh" > /rootfs/etc/passwd && \
    echo "nobody:x:65534:65534:nobody:/:/sbin/nologin" >> /rootfs/etc/passwd && \
    echo "root:x:0:" > /rootfs/etc/group && \
    echo "nobody:x:65534:" >> /rootfs/etc/group

### ETAPA 2: Compilación Motor WASM
FROM emscripten/emsdk:latest AS wasm-build
WORKDIR /app
COPY wasm/ ./wasm/
RUN cd wasm/src && make

### ETAPA 3: Construcción Frontend (Svelte + Bun)
FROM oven/bun:1.2-slim AS svelte-build
WORKDIR /app
COPY frontend/package.json frontend/bun.lockb* ./
RUN bun install --frozen-lockfile
COPY frontend/ ./
COPY --from=wasm-build /app/wasm/dist/*.wasm ./static/
COPY --from=wasm-build /app/wasm/dist/*.js ./static/
RUN bun run build

### ETAPA 4: IMAGEN FINAL
FROM scratch
# Copy the binary and mime types directly from the builder's system install
COPY --from=nginx-builder /usr/sbin/nginx /usr/sbin/nginx
COPY --from=nginx-builder /etc/nginx/mime.types /etc/nginx/mime.types

# Copy our prepared rootfs structure (passwd, group, logs, tmp)
COPY --from=nginx-builder /rootfs/ / 

# Copy config, certs, and frontend
COPY conf/nginx.conf /etc/nginx/nginx.conf
COPY certs/server.crt /etc/nginx/certs/server.crt
COPY certs/server.key /etc/nginx/certs/server.key
COPY --from=svelte-build /app/build /usr/share/nginx/html

EXPOSE 443
ENTRYPOINT ["/usr/sbin/nginx", "-g", "daemon off;"]