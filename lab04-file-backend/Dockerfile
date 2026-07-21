# Lab 4 — PHP 7.4 + rasplab, built-in server. /data holds both the note and the
# taint sidecar (/data/.taint.jsonl).
FROM php:7.4-cli
ARG SRC=ext
WORKDIR /build
COPY ${SRC}/ /build/
RUN phpize \
 && ./configure --enable-rasplab=shared \
 && make -j"$(nproc)" \
 && make install \
 && echo "extension=rasplab.so" > "$(php -r 'echo PHP_CONFIG_FILE_SCAN_DIR;')/rasplab.ini"
RUN php -m | grep -qi rasplab && echo "rasplab loaded OK"
RUN mkdir -p /data && chmod 777 /data
COPY app/ /app/
WORKDIR /app
EXPOSE 8080
CMD ["php", "-S", "0.0.0.0:8080", "-t", "/app"]
