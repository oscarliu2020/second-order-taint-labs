# Lab 1 — PHP 7.4 + your homegrown rasplab extension.
# Builds whatever's in ext/ (your work). To grade the reference impl, see verify.sh.
FROM php:7.4-cli

ARG SRC=ext
WORKDIR /build
COPY ${SRC}/ /build/

# phpize -> configure -> make -> install, then drop one line of ini to load it.
# NOTE: =shared is mandatory — a static build has no get_module() and won't load.
RUN phpize \
 && ./configure --enable-rasplab=shared \
 && make -j"$(nproc)" \
 && make install \
 && echo "extension=rasplab.so" > "$(php -r 'echo PHP_CONFIG_FILE_SCAN_DIR;')/rasplab.ini"

# Sanity: is it actually loaded?
RUN php -m | grep -qi rasplab && echo "rasplab loaded OK"

COPY app/ /app/
WORKDIR /app

# Run the target; the opcode trace shows up on stderr.
CMD ["php", "demo.php"]
