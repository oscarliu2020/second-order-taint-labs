/* ============================================================
 * rasplab — Lab 3: Stored XSS Failure
 *
 *   Request #1  save.php:  $_GET  ─▶ file_put_contents()   (taint enters storage)
 *   Request #2  view.php:  file_get_contents() ─▶ echo     (detector goes BLIND)
 *
 * Everything from Lab 2 (taint set, $_GET marking, echo sink) is carried over
 * and PROVIDED. New skill this lab: hooking an *internal function* to watch a
 * storage sink. You'll prove taint reaches storage on write — and then watch
 * the echo on the next request fail to fire. That blind spot is second-order.
 *
 * You fill one TODO(lab3-1): detect tainted data hitting file_put_contents().
 * ============================================================ */

#include "php.h"
#include "php_rasplab.h"
#include "zend_execute.h"
#include "zend_vm_opcodes.h"
#include "zend_compile.h"
#include "php_variables.h"
#include "SAPI.h"

/* ---- taint set + helpers (from Lab 2, provided) ------------------------- */
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
static void tl_taint_superglobal(const char *name, size_t name_len, int track_var) {
    zend_is_auto_global_str((char *) name, name_len);
    tl_taint_array(&PG(http_globals)[track_var]);
}

/* ---- ECHO sink (from Lab 2, provided) ---------------------------------- */
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

/* ---- NEW: hook the internal function file_put_contents ------------------ *
 * We save the original handler, install ours, and chain to the original.    */
static void (*orig_fpc)(INTERNAL_FUNCTION_PARAMETERS) = NULL;

static void tl_file_put_contents(INTERNAL_FUNCTION_PARAMETERS) {
    /* file_put_contents(string $filename, mixed $data, ...) */
    zval *z_name = ZEND_NUM_ARGS() >= 1 ? ZEND_CALL_ARG(execute_data, 1) : NULL;
    zval *z_data = ZEND_NUM_ARGS() >= 2 ? ZEND_CALL_ARG(execute_data, 2) : NULL;
    if (z_name) ZVAL_DEREF(z_name);
    if (z_data) ZVAL_DEREF(z_data);

    /* ============================================================
     * TODO(lab3-1): if the DATA being written is a tainted string, log it.
     *   - guard: z_data non-NULL and Z_TYPE_P(z_data) == IS_STRING
     *   - tainted? tl_is_tainted(Z_STR_P(z_data))
     *   - the storage "location" is the filename: Z_STR_P(z_name) (may be NULL)
     *   - log format (verify.sh greps "[STORE-TAINT]" + the payload):
     *       [STORE-TAINT] loc=<filename> data="<content>"
     *   This proves taint reached storage. (We are NOT persisting it yet —
     *    that's Lab 4. Right now it just... vanishes across the request.)
     * ============================================================ */


    /* chain to the real file_put_contents */
    orig_fpc(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

/* Install a handler over an internal function, returning the original. Provided. */
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
    tl_hook_function("file_put_contents", sizeof("file_put_contents") - 1,
                     tl_file_put_contents, &orig_fpc);
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
