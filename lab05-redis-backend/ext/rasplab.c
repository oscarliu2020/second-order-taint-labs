/* ============================================================
 * rasplab — Lab 5: Redis Backend (taint that crosses services)
 *
 *   service-a  save.php:  file_put_contents(tainted) ─▶ SETEX taint:<loc> <uuid>
 *   service-b  view.php:  file_get_contents()        ─▶ GET   taint:<loc>
 *                                                    ─▶ re-mark ─▶ echo [ALERT]
 *
 * Same export/recover pattern as Lab 4 — but the carrier is now Redis instead of
 * a local file. That one swap buys you: cross-*service* taint (any box that can
 * reach Redis), a TTL (taint expires), and no unbounded local sidecar.
 *
 * You fill two TODO(lab5-*):
 *   (1) EXPORT  : SETEX the taint id under a location-derived key
 *   (2) RECOVER : GET it back; if present, re-mark the string
 *
 * Provided: everything from Lab 4 + a tiny Redis client (raw socket + RESP).
 * ============================================================ */

#include "php.h"
#include "php_rasplab.h"
#include "zend_execute.h"
#include "zend_vm_opcodes.h"
#include "zend_compile.h"
#include "php_variables.h"
#include "SAPI.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>

#define TAINT_TTL "300"   /* seconds — taint self-destructs */

/* ---- taint set + helpers (provided) ------------------------------------ */
static HashTable RASP_tainted;
static inline void tl_mark(zend_string *s) {
    if (s) zend_hash_index_add_empty_element(&RASP_tainted, (zend_ulong)(uintptr_t) s);
}
static inline int tl_is_tainted(zend_string *s) {
    return s && zend_hash_index_exists(&RASP_tainted, (zend_ulong)(uintptr_t) s);
}
static zval *tl_get_zval(zend_execute_data *execute_data, const znode_op *node, zend_uchar type) {
    if (type == IS_CONST) return RT_CONSTANT(EX(opline), (*node));
    if (type == IS_CV || type == IS_VAR || type == IS_TMP_VAR) return EX_VAR(node->var);
    return NULL;
}
static void tl_taint_array(zval *arr) {
    if (!arr || Z_TYPE_P(arr) != IS_ARRAY) return;
    zval *val;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(arr), val) {
        ZVAL_DEREF(val);
        if (Z_TYPE_P(val) == IS_STRING) tl_mark(Z_STR_P(val));
    } ZEND_HASH_FOREACH_END();
}
static void tl_taint_superglobal(const char *name, size_t len, int tv) {
    zend_is_auto_global_str((char *) name, len);
    tl_taint_array(&PG(http_globals)[tv]);
}
static void tl_uuid(char *buf, size_t n) {
    static unsigned ctr = 0;
    snprintf(buf, n, "%x-%x", (unsigned) getpid(), ctr++);
}
static void tl_key(char *buf, size_t n, const char *loc) {
    snprintf(buf, n, "taint:%s", loc);   /* location-derived Redis key */
}

/* ---- tiny Redis client: raw TCP + RESP (provided) ---------------------- *
 * tl_redis_command(argc, argv, reply, reply_sz) sends one RESP command and    *
 * captures the raw reply. Returns bytes read, or -1 on failure.               *
 * Host from $REDIS_HOST (default "redis"), port 6379.                         */
static int tl_redis_send(const char *payload, size_t n, char *reply, size_t rn) {
    const char *host = getenv("REDIS_HOST"); if (!host) host = "redis";
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "6379", &hints, &res) != 0 || !res) return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol), rc = -1;
    if (fd >= 0) {
        struct timeval tv = { 2, 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (connect(fd, res->ai_addr, res->ai_addrlen) == 0 &&
            write(fd, payload, n) == (ssize_t) n) {
            ssize_t r = read(fd, reply, rn - 1);
            if (r >= 0) { reply[r] = '\0'; rc = (int) r; }
        }
        close(fd);
    }
    freeaddrinfo(res);
    return rc;
}
static int tl_redis_command(int argc, const char **argv, char *reply, size_t rn) {
    char buf[4096]; int off = 0;
    off += snprintf(buf + off, sizeof(buf) - off, "*%d\r\n", argc);
    for (int i = 0; i < argc; i++)
        off += snprintf(buf + off, sizeof(buf) - off, "$%zu\r\n%s\r\n",
                        strlen(argv[i]), argv[i]);
    return tl_redis_send(buf, (size_t) off, reply, rn);
}

/* ---- ECHO sink (provided) ---------------------------------------------- */
static int tl_echo_handler(zend_execute_data *execute_data) {
    const zend_op *opline = EX(opline);
    zval *op1 = tl_get_zval(execute_data, &opline->op1, opline->op1_type);
    if (op1) ZVAL_DEREF(op1);
    if (op1 && Z_TYPE_P(op1) == IS_STRING && tl_is_tainted(Z_STR_P(op1))) {
        fprintf(stderr, "[ALERT] tainted -> ECHO: \"%s\" (line=%d)\n",
                ZSTR_VAL(Z_STR_P(op1)), (int) opline->lineno);
    }
    return ZEND_USER_OPCODE_DISPATCH;
}

/* ---- EXPORT hook: file_put_contents ------------------------------------ */
static void (*orig_fpc)(INTERNAL_FUNCTION_PARAMETERS) = NULL;
static void tl_file_put_contents(INTERNAL_FUNCTION_PARAMETERS) {
    zval *z_name = ZEND_NUM_ARGS() >= 1 ? ZEND_CALL_ARG(execute_data, 1) : NULL;
    zval *z_data = ZEND_NUM_ARGS() >= 2 ? ZEND_CALL_ARG(execute_data, 2) : NULL;
    if (z_name) ZVAL_DEREF(z_name);
    if (z_data) ZVAL_DEREF(z_data);

    if (z_data && Z_TYPE_P(z_data) == IS_STRING && tl_is_tainted(Z_STR_P(z_data))
        && z_name && Z_TYPE_P(z_name) == IS_STRING) {
        char key[600], id[64], reply[256];
        tl_key(key, sizeof(key), ZSTR_VAL(Z_STR_P(z_name)));
        tl_uuid(id, sizeof(id));

        /* ============================================================
         * TODO(lab5-1): EXPORT to Redis. Record the taint id under `key`,
         * with a TTL, so any service that reads this location can recover it.
         *   - command: SETEX <key> <TTL> <id>
         *   - build argv and call tl_redis_command:
         *       const char *argv[] = { "SETEX", key, TAINT_TTL, id };
         *       tl_redis_command(4, argv, reply, sizeof(reply));
         *   - (nice to log:) fprintf(stderr, "[EXPORT] %s -> %s\n", key, id);
         * ============================================================ */


    }
    orig_fpc(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

/* ---- RECOVER hook: file_get_contents ----------------------------------- */
static void (*orig_fgc)(INTERNAL_FUNCTION_PARAMETERS) = NULL;
static void tl_file_get_contents(INTERNAL_FUNCTION_PARAMETERS) {
    zval *z_name = ZEND_NUM_ARGS() >= 1 ? ZEND_CALL_ARG(execute_data, 1) : NULL;
    if (z_name) ZVAL_DEREF(z_name);

    orig_fgc(INTERNAL_FUNCTION_PARAM_PASSTHRU);  /* real read fills return_value */

    if (z_name && Z_TYPE_P(z_name) == IS_STRING
        && return_value && Z_TYPE_P(return_value) == IS_STRING) {
        char key[600], reply[256];
        tl_key(key, sizeof(key), ZSTR_VAL(Z_STR_P(z_name)));

        /* ============================================================
         * TODO(lab5-2): RECOVER from Redis. GET the key; if a value exists,
         * the string we just read is tainted → mark it.
         *   - command: GET <key>
         *       const char *argv[] = { "GET", key };
         *       tl_redis_command(2, argv, reply, sizeof(reply));
         *   - RESP reply: a hit looks like "$<len>\r\n<id>\r\n";
         *                 a miss is "$-1\r\n".
         *     So: tainted if reply[0]=='$' && strncmp(reply,"$-1",3)!=0
         *   - on hit: tl_mark(Z_STR_P(return_value));
         *   - (nice to log:) fprintf(stderr, "[RECOVER] %s\n", key);
         * ============================================================ */


    }
}

/* ---- function-hook install (provided) ---------------------------------- */
static void tl_hook_function(const char *name, size_t len,
                             void (*ours)(INTERNAL_FUNCTION_PARAMETERS),
                             void (**save)(INTERNAL_FUNCTION_PARAMETERS)) {
    zend_function *fn = zend_hash_str_find_ptr(CG(function_table), name, len);
    if (fn && fn->type == ZEND_INTERNAL_FUNCTION) {
        *save = fn->internal_function.handler;
        fn->internal_function.handler = ours;
    }
}

/* ---- lifecycle (provided) ---------------------------------------------- */
PHP_MINIT_FUNCTION(rasplab) {
    zend_set_user_opcode_handler(ZEND_ECHO, tl_echo_handler);
    tl_hook_function("file_put_contents", sizeof("file_put_contents") - 1, tl_file_put_contents, &orig_fpc);
    tl_hook_function("file_get_contents", sizeof("file_get_contents") - 1, tl_file_get_contents, &orig_fgc);
    return SUCCESS;
}
PHP_MSHUTDOWN_FUNCTION(rasplab) {
    zend_set_user_opcode_handler(ZEND_ECHO, NULL);
    return SUCCESS;
}
PHP_RINIT_FUNCTION(rasplab) {
    zend_hash_init(&RASP_tainted, 16, NULL, NULL, 0);
    tl_taint_superglobal("_GET",  sizeof("_GET") - 1,  TRACK_VARS_GET);
    tl_taint_superglobal("_POST", sizeof("_POST") - 1, TRACK_VARS_POST);
    return SUCCESS;
}
PHP_RSHUTDOWN_FUNCTION(rasplab) {
    zend_hash_destroy(&RASP_tainted);
    return SUCCESS;
}

zend_module_entry rasplab_module_entry = {
    STANDARD_MODULE_HEADER, "rasplab", NULL,
    PHP_MINIT(rasplab), PHP_MSHUTDOWN(rasplab),
    PHP_RINIT(rasplab), PHP_RSHUTDOWN(rasplab), NULL,
    PHP_RASPLAB_VERSION, STANDARD_MODULE_PROPERTIES
};
/* labs always build as a shared module — emit get_module() unconditionally */
ZEND_GET_MODULE(rasplab)
