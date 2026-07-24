#!/bin/sh
set -e
mkdir -p /data
export FLAG="FLAG{$(head -c 8 /dev/urandom | od -An -tx1 | tr -d ' \n')}"
echo "$FLAG" > /flag.txt
php /app/init.php
exec php -S 0.0.0.0:8090 -t /app /app/router.php
