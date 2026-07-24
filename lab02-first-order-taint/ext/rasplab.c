/* ============================================================
 * rasplab — Lab 2: First Order Taint
 *
 *   Input ($_GET) ──mark──▶ echo ──detect──▶ ALERT
 *
 * Building on Lab 1's opcode hook: treat user input as a taint SOURCE, and
 * scream when it reaches the echo SINK. This is taint-tracking in its most
 * primitive form — one bit per value: tainted or not.
 *
 * You fill two TODO(lab2-*) blocks:
 *   (1) tl_taint_array()  — mark strings in an array as tainted  (Source)
 *   (2) tl_echo_handler() — alert when echoing tainted data      (Sink)
 *
 * The rest (taint set, mark/is_tainted, operand fetch, $_GET auto-marking,
 * handler registration) is handed to you.
 * ============================================================ */

#include "php.h"
#include "php_rasplab.h"
#include "zend_execute.h"
#include "zend_vm_opcodes.h"
#include "zend_compile.h"      /* RT_CONSTANT */
#include "php_variables.h"     /* TRACK_VARS_GET / TRACK_VARS_POST */
#include "SAPI.h"

/* ------------------------------------------------------------
 * The taint set: a bag of "tainted zend_string* addresses".
 * Crudest model that works — identity == pointer value.
 * (Caveat: once a string is freed its address can be recycled → false
 *  positives. Later labs swap this for a proper ID model. Live with it now.)
 * Provided.
 * ------------------------------------------------------------ */
static HashTable RASP_tainted;

static inline void tl_mark(zend_string *s) {
    if (s) zend_hash_index_add_empty_element(&RASP_tainted, (zend_ulong)(uintptr_t) s);
}
static inline int tl_is_tainted(zend_string *s) {
    return s && zend_hash_index_exists(&RASP_tainted, (zend_ulong)(uintptr_t) s);
}

/* ------------------------------------------------------------
 * Fetch the zval behind an opcode operand. Fiddly plumbing — provided, don't
 * sink time here.
 * ------------------------------------------------------------ */
static zval *tl_get_zval(zend_execute_data *execute_data, const znode_op *node, zend_uchar type) {
    if (type == IS_CONST) return RT_CONSTANT(EX(opline), (*node));
    if (type == IS_CV || type == IS_VAR || type == IS_TMP_VAR) return EX_VAR(node->var);
    return NULL; /* IS_UNUSED */
}

/* ------------------------------------------------------------
 * Source: mark every string value in an array (e.g. $_GET) as tainted.
 * ------------------------------------------------------------ */
static void tl_taint_array(zval *arr) {
    if (!arr || Z_TYPE_P(arr) != IS_ARRAY) return;

    zval *val;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(arr), val) {
        ZVAL_DEREF(val);  /* follow references down to the real value */

        /* ============================================================
         * TODO(lab2-1): if val is a string, mark it tainted.
         *   - type check : Z_TYPE_P(val) == IS_STRING
         *   - get string : Z_STR_P(val)
         *   - mark       : tl_mark(...)
         *   (bonus: val can be a nested array — strings only is fine for now)
         * ============================================================ */
        if(Z_TYPE_P(val) == IS_STRING) {
            tl_mark(Z_STR_P(val));
        }
    } ZEND_HASH_FOREACH_END();
}

/* Force-init a (lazy) superglobal like $_GET, then taint its contents. Provided. */
static void tl_taint_superglobal(const char *name, size_t name_len, int track_var) {
    zend_is_auto_global_str((char *) name, name_len);      /* materialize $_GET */
    zval *arr = &PG(http_globals)[track_var];
    tl_taint_array(arr);
}

/* ------------------------------------------------------------
 * Sink: the ECHO handler. Alert when we echo a tainted string.
 * ------------------------------------------------------------ */
static int tl_echo_handler(zend_execute_data *execute_data) {
    const zend_op *opline = EX(opline);
    zval *op1 = tl_get_zval(execute_data, &opline->op1, opline->op1_type);
    if (op1) ZVAL_DEREF(op1);

    /* ============================================================
     * TODO(lab2-2): if we're echoing a tainted string, log an alert to stderr.
     *   - op1 may be NULL or non-string → guard first
     *   - is string  : Z_TYPE_P(op1) == IS_STRING
     *   - is tainted : tl_is_tainted(Z_STR_P(op1))
     *   - alert format (verify.sh greps "[ALERT]" + the echoed content):
     *       [ALERT] tainted -> ECHO: "<content>" (line=<lineno>)
     *     content: ZSTR_VAL(Z_STR_P(op1)), line: opline->lineno
     * ============================================================ */

    if(op1 && Z_TYPE_P(op1) == IS_STRING && tl_is_tainted(Z_STR_P(op1))) {
        fprintf(stderr, "[ALERT] tainted -> ECHO: \"%s\" (line=%d)\n", ZSTR_VAL(Z_STR_P(op1)), opline->lineno);
    }
    return ZEND_USER_OPCODE_DISPATCH;  /* let the real echo run */
}

/* ------------------------------------------------------------
 * Lifecycle. Provided.
 * ------------------------------------------------------------ */
PHP_MINIT_FUNCTION(rasplab) {
    /* register the ECHO handler (you learned this in Lab 1) */
    zend_set_user_opcode_handler(ZEND_ECHO, tl_echo_handler);
    return SUCCESS;
}
PHP_MSHUTDOWN_FUNCTION(rasplab) {
    zend_set_user_opcode_handler(ZEND_ECHO, NULL);
    return SUCCESS;
}
PHP_RINIT_FUNCTION(rasplab) {
    zend_hash_init(&RASP_tainted, 16, NULL, NULL, 0);
    /* user input is the taint source */
    tl_taint_superglobal("_GET",  sizeof("_GET") - 1,  TRACK_VARS_GET);
    tl_taint_superglobal("_POST", sizeof("_POST") - 1, TRACK_VARS_POST);
    return SUCCESS;
}
PHP_RSHUTDOWN_FUNCTION(rasplab) {
    zend_hash_destroy(&RASP_tainted);
    return SUCCESS;
}

zend_module_entry rasplab_module_entry = {
    STANDARD_MODULE_HEADER,
    "rasplab",
    NULL,
    PHP_MINIT(rasplab),
    PHP_MSHUTDOWN(rasplab),
    PHP_RINIT(rasplab),
    PHP_RSHUTDOWN(rasplab),
    NULL,
    PHP_RASPLAB_VERSION,
    STANDARD_MODULE_PROPERTIES
};

/* labs always build as a shared module — emit get_module() unconditionally */
ZEND_GET_MODULE(rasplab)
