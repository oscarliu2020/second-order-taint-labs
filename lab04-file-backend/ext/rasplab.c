/* ============================================================
 * rasplab — Lab 4: File Backend (persist the taint, not the data)
 *
 *   save.php:  file_put_contents(tainted)  ─▶ EXPORT taint record → sidecar
 *   view.php:  file_get_contents()         ─▶ RECOVER taint from sidecar → re-mark
 *                                          ─▶ echo → [ALERT]  🎯
 *
 * The fix for Lab 3's blind spot: when tainted data crosses into storage, write
 * a little record keyed by the *storage location*. When data comes back out,
 * look the location up and re-taint the fresh string. Taint now survives the
 * request boundary because it lives on disk, not in a dead process's heap.
 *
 * You fill three TODO(lab4-*):
 *   (1) EXPORT decision   in the file_put_contents hook
 *   (2) RECOVER decision  in the file_get_contents hook
 *   (3) tl_persist_write(): actually write the record OUT to an external file
 *
 * Provided: taint set, $_GET marking, echo sink, function-hook install, a uuid
 * generator, and the sidecar READ helper (lookup/parse). You write the file output.
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
#include <unistd.h>

#define TAINT_STORE "/data/.taint.jsonl"

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

/* ---- data model + sidecar persistence ---------------------------------- *
 * One JSON object per line: {"loc":"...","uuid":"...","src":"..."}          */
static void tl_uuid(char *buf, size_t n) {                 /* provided */
    static unsigned ctr = 0;
    snprintf(buf, n, "%x-%x", (unsigned) getpid(), ctr++);
}

/* EXPORT sink: write ONE taint record out to the external sidecar file. */
static void tl_persist_write(const char *loc, const char *uuid, const char *src) {
    /* ============================================================
     * TODO(lab4-3): write the taint record OUT to an external file.
     * This is the whole point of a "backend" — the taint has to leave this
     * process and land on disk so a *later* request can read it back.
     *   - open the sidecar in APPEND mode:  FILE *f = fopen(TAINT_STORE, "a");
     *     (TAINT_STORE is defined near the top: "/data/.taint.jsonl")
     *   - bail if fopen failed (f == NULL)
     *   - write exactly one JSON line for this record, e.g.:
     *       fprintf(f, "{\"loc\":\"%s\",\"uuid\":\"%s\",\"src\":\"%s\"}\n",
     *               loc, uuid, src);
     *   - fclose(f)
     * Append (not "w") so multiple records accumulate across requests.
     * ============================================================ */
    FILE *f=fopen(TAINT_STORE,"a");
    if(f==NULL) return;
    fprintf(f,"{\"loc\":\"%s\",\"uuid\":\"%s\",\"src\":\"%s\"}\n",loc,uuid,src);
    fclose(f);
}

/* RECOVER: return 1 if any record exists for this location. (provided) */
static int tl_persist_lookup(const char *loc) {
    FILE *f = fopen(TAINT_STORE, "r");
    if (!f) return 0;
    char needle[512], line[2048];
    snprintf(needle, sizeof(needle), "\"loc\":\"%s\"", loc);
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, needle)) { found = 1; break; }
    }
    fclose(f);
    return found;
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

    /* ============================================================
     * TODO(lab4-1): EXPORT. If tainted data is being written, persist a taint
     * record keyed by the storage location so a later request can recover it.
     *   - tainted?  z_data is IS_STRING && tl_is_tainted(Z_STR_P(z_data))
     *   - location: ZSTR_VAL(Z_STR_P(z_name))   (the filename)
     *   - mint an id: char id[64]; tl_uuid(id, sizeof(id));
     *   - persist:  tl_persist_write(loc, id, "user_input");
     *   - (nice to log too:) fprintf(stderr, "[EXPORT] loc=%s uuid=%s\n", loc, id);
     * ============================================================ */

        if(z_data&&Z_TYPE_P(z_data)==IS_STRING && tl_is_tainted(Z_STR_P(z_data))) {
            char id[64];
            tl_uuid(id,sizeof(id));
            tl_persist_write(ZSTR_VAL(Z_STR_P(z_name)),id,"user_input");
            fprintf(stderr, "[EXPORT] loc=%s uuid=%s\n",ZSTR_VAL(Z_STR_P(z_name)),id);
        }
    orig_fpc(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

/* ---- RECOVER hook: file_get_contents ----------------------------------- */
static void (*orig_fgc)(INTERNAL_FUNCTION_PARAMETERS) = NULL;
static void tl_file_get_contents(INTERNAL_FUNCTION_PARAMETERS) {
    zval *z_name = ZEND_NUM_ARGS() >= 1 ? ZEND_CALL_ARG(execute_data, 1) : NULL;
    if (z_name) ZVAL_DEREF(z_name);

    /* run the real read first — it fills return_value with the file contents */
    orig_fgc(INTERNAL_FUNCTION_PARAM_PASSTHRU);

    /* ============================================================
     * TODO(lab4-2): RECOVER. If this location has a taint record, the string we
     * just read back is tainted too — mark it, so the downstream echo fires.
     *   - location: ZSTR_VAL(Z_STR_P(z_name))  (guard z_name is IS_STRING)
     *   - has taint? tl_persist_lookup(loc)
     *   - the value just read is in return_value; if it's IS_STRING, mark it:
     *       tl_mark(Z_STR_P(return_value));
     *   - (nice to log:) fprintf(stderr, "[RECOVER] loc=%s\n", loc);
     * ============================================================ */
    if(z_name&&Z_TYPE_P(z_name)==IS_STRING && tl_persist_lookup(ZSTR_VAL(Z_STR_P(z_name)))) {
        if(Z_TYPE_P(return_value)==IS_STRING) tl_mark(Z_STR_P(return_value));
        fprintf(stderr, "[RECOVER] loc=%s\n",ZSTR_VAL(Z_STR_P(z_name)));
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
