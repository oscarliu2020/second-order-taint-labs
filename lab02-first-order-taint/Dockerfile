# Lab 2 — PHP 7.4 + rasplab, served via the built-in web server so $_GET exists.
FROM php:7.4-cli

ARG SRC=ext
WORKDIR /build
COPY ${SRC}/ /build/

# =shared is mandatory (static build has no get_module()).
RUN phpize \
 && ./configure --enable-rasplab=shared \
 && make -j"$(nproc)" \
 && make install \
 && echo "extension=rasplab.so" > "$(php -r 'echo PHP_CONFIG_FILE_SCAN_DIR;')/rasplab.ini"

RUN php -m | grep -qi rasplab && echo "rasplab loaded OK"

COPY app/ /app/
WORKDIR /app
EXPOSE 8080

# Built-in server: this is what populates $_GET. Alerts + access log go to stderr
# (visible via `docker logs`).
CMD ["php", "-S", "0.0.0.0:8080", "-t", "/app"]
