/* ============================================================
 * rasplab — Lab 1: Opcode Handler
 *
 * Mission: wedge yourself into the Zend VM and snitch on every opcode it
 *          executes. This is how every RASP starts — own the dispatch loop,
 *          and the rest is just deciding what to log/block. Here we only log.
 *
 * You fill two TODO(lab1-*) blocks. Everything else (module boilerplate,
 * handler skeleton, teardown) is already wired up.
 * ============================================================ */

#include "php.h"
#include "php_rasplab.h"
#include "zend_execute.h"      /* zend_set_user_opcode_handler, EX() */
#include "zend_vm_opcodes.h"   /* ZEND_ASSIGN / ZEND_ECHO ... + zend_get_opcode_name() */

/* The opcodes we tap. Note: function calls aren't one opcode — internal funcs
 * are ZEND_DO_ICALL, userland is ZEND_DO_UCALL, by-name is ZEND_DO_FCALL — so
 * we hook the whole call family. */
static const zend_uchar tl_watch[] = {
    ZEND_ASSIGN,           /* variable assignment        */
    ZEND_CONCAT,           /* string concatenation       */
    ZEND_ECHO,             /* output (our future Sink)    */
    ZEND_DO_FCALL,         /* call (by-name)             */
    ZEND_DO_ICALL,         /* internal fn (strtoupper…)  */
    ZEND_DO_UCALL,         /* userland fn                */
    ZEND_DO_FCALL_BY_NAME,
};

/* ------------------------------------------------------------
 * Our user opcode handler. The VM calls this *before* the real handler for
 * any opcode we've claimed.
 * ------------------------------------------------------------ */
static int tl_trace_handler(zend_execute_data *execute_data)
{
    const zend_op *opline = EX(opline);

    /* ============================================================
     * TODO(lab1-1): dump the current opcode to stderr.
     *   - opcode mnemonic: zend_get_opcode_name(opline->opcode)
     *   - source line    : opline->lineno
     *   - write to stderr (fprintf(stderr, ...)) — NOT echo/php_printf,
     *     or you'll poison the program's stdout.
     *
     *   Expected line format (verify.sh greps for this):
     *     [opcode] ZEND_ECHO         line=5
     * ============================================================ */


    /* Hand control back to the *original* handler. Behaviour stays identical —
     * we only observed. */
    return ZEND_USER_OPCODE_DISPATCH;
}

/* ------------------------------------------------------------
 * Module init: claim the opcodes.
 * ------------------------------------------------------------ */
PHP_MINIT_FUNCTION(rasplab)
{
    /* ============================================================
     * TODO(lab1-2): register tl_trace_handler for every opcode in tl_watch[].
     *   - API: zend_set_user_opcode_handler(zend_uchar opcode, handler)
     *   - loop over tl_watch[]; length = sizeof(tl_watch)/sizeof(tl_watch[0])
     * ============================================================ */


    return SUCCESS;
}

/* ------------------------------------------------------------
 * Module shutdown: release the opcodes (set handlers back to NULL) so we don't
 * haunt other modules. Already done.
 * ------------------------------------------------------------ */
PHP_MSHUTDOWN_FUNCTION(rasplab)
{
    size_t i;
    for (i = 0; i < sizeof(tl_watch) / sizeof(tl_watch[0]); i++) {
        zend_set_user_opcode_handler(tl_watch[i], NULL);
    }
    return SUCCESS;
}

/* ------------------------------------------------------------
 * Module registration table. Don't touch.
 * ------------------------------------------------------------ */
zend_module_entry rasplab_module_entry = {
    STANDARD_MODULE_HEADER,
    "rasplab",
    NULL,                       /* functions           */
    PHP_MINIT(rasplab),         /* MINIT               */
    PHP_MSHUTDOWN(rasplab),     /* MSHUTDOWN           */
    NULL,                       /* RINIT               */
    NULL,                       /* RSHUTDOWN           */
    NULL,                       /* MINFO               */
    PHP_RASPLAB_VERSION,
    STANDARD_MODULE_PROPERTIES
};

/* labs always build as a shared module — emit get_module() unconditionally */
ZEND_GET_MODULE(rasplab)
