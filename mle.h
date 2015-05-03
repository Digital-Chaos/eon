#ifndef __MLE_H
#define __MLE_H

#include <stdint.h>
#include <termbox.h>
#include <limits.h>
#include "uthash.h"
#include "mlbuf.h"

// Typedefs
typedef struct editor_s editor_t; // A container for editor-wide globals
typedef struct bview_s bview_t; // A view of a buffer
typedef struct bview_rect_s bview_rect_t; // A rectangle in bview with a default styling
typedef struct bview_listener_s bview_listener_t; // A listener to buffer events in a bview
typedef void (*bview_listener_cb_t)(bview_t* bview, baction_t* action, void* udata); // A bview_listener_t callback
typedef struct cursor_s cursor_t; // A cursor (insertion mark + selection bound mark) in a buffer
typedef struct loop_context_s loop_context_t; // Context for a single _editor_loop
typedef struct cmd_context_s cmd_context_t; // Context for a single command invocation
typedef int (*cmd_func_t)(cmd_context_t* ctx); // A command function
typedef struct cmd_funcref_s cmd_funcref_t; // A reference to a command function
typedef struct kinput_s kinput_t; // A single key input (similar to a tb_event from termbox)
typedef struct kmacro_s kmacro_t; // A sequence of kinputs and a name
typedef struct kmap_s kmap_t; // A map of keychords to functions
typedef struct kmap_node_s kmap_node_t; // A node in a list of keymaps
typedef struct kbinding_def_s kbinding_def_t; // A definition of a keymap
typedef struct kbinding_s kbinding_t; // A single binding in a keymap
typedef struct syntax_s syntax_t; // A syntax definition
typedef struct syntax_node_s syntax_node_t; // A node in a linked list of syntaxes
typedef struct srule_def_s srule_def_t; // A definition of a syntax
typedef struct async_proc_s async_proc_t; // An asynchronous process
typedef void (*async_proc_cb_t)(async_proc_t* self, char* buf, size_t buf_len, int is_error, int is_eof, int is_timeout); // An async_proc_t callback
typedef struct tb_event tb_event_t; // A termbox event

// kinput_t
struct kinput_s {
    uint8_t mod;
    uint32_t ch;
    uint16_t key;
};

// bview_rect_t
struct bview_rect_s {
    int x;
    int y;
    int w;
    int h;
    uint16_t fg;
    uint16_t bg;
};

// editor_t
struct editor_s {
    int w;
    int h;
    bview_t* top_bviews;
    bview_t* all_bviews;
    bview_t* active;
    bview_t* active_edit;
    bview_t* active_edit_root;
    bview_t* status;
    bview_t* prompt;
    bview_rect_t rect_edit;
    bview_rect_t rect_status;
    bview_rect_t rect_prompt;
    syntax_t* syntax_map;
    int is_display_disabled;
    kmacro_t* macro_map;
    kinput_t macro_toggle_key;
    kmacro_t* macro_record;
    kmacro_t* macro_apply;
    size_t macro_apply_input_index;
    int is_recording_macro;
    cmd_funcref_t* func_map;
    kmap_t* kmap_map;
    kmap_t* kmap_normal;
    kmap_t* kmap_prompt_input;
    kmap_t* kmap_prompt_yn;
    kmap_t* kmap_prompt_yna;
    kmap_t* kmap_prompt_ok;
    kmap_t* kmap_prompt_isearch;
    kmap_t* kmap_prompt_menu;
    kmap_t* kmap_menu;
    char* kmap_init_name;
    kmap_t* kmap_init;
    async_proc_t* async_procs;
    FILE* tty;
    int ttyfd;
    char* syntax_override;
    int linenum_type;
    int tab_width;
    int tab_to_space;
    int highlight_bracket_pairs;
    int color_col;
    int viewport_scope_x; // TODO cli option
    int viewport_scope_y; // TODO cli option
    loop_context_t* loop_ctx;
    int loop_depth;
    bint_t startup_linenum;
    int is_in_init;
    #define MLE_ERRSTR_SIZE 256
    char errstr[MLE_ERRSTR_SIZE];
    int exit_code;
};

// srule_def_t
struct srule_def_s {
    char* re;
    char* re_end;
    uint16_t fg;
    uint16_t bg;
};

// syntax_node_t
struct syntax_node_s {
    srule_t* srule;
    syntax_node_t* next;
    syntax_node_t* prev;
};

// syntax_t
struct syntax_s {
    char* name;
    char* path_pattern;
    srule_node_t* srules;
    UT_hash_handle hh;
};

// bview_t
struct bview_s {
    #define MLE_BVIEW_TYPE_EDIT 0
    #define MLE_BVIEW_TYPE_STATUS 1
    #define MLE_BVIEW_TYPE_PROMPT 2
    editor_t* editor;
    int x;
    int y;
    int w;
    int h;
    int type;
    int linenum_width;
    int abs_linenum_width;
    int rel_linenum_width;
    bview_rect_t rect_caption;
    bview_rect_t rect_lines;
    bview_rect_t rect_margin_left;
    bview_rect_t rect_buffer;
    bview_rect_t rect_margin_right;
    buffer_t* buffer;
    bint_t viewport_x;
    bint_t viewport_x_vcol;
    bint_t viewport_y;
    bline_t* viewport_bline;
    int viewport_scope_x;
    int viewport_scope_y;
    bview_t* split_parent;
    bview_t* split_child;
    float split_factor;
    int split_is_vertical;
    char* prompt_str;
    char* path;
    kmap_node_t* kmap_stack;
    kmap_node_t* kmap_tail;
    cursor_t* cursors;
    cursor_t* active_cursor;
    char* last_search;
    srule_t* isearch_rule;
    int tab_width;
    int tab_to_space;
    syntax_t* syntax;
    async_proc_t* async_proc;
    cmd_func_t menu_callback;
    int is_menu;
    char init_cwd[PATH_MAX + 1];
    bview_listener_t* listeners;
    bview_t* top_next;
    bview_t* top_prev;
    bview_t* all_next;
    bview_t* all_prev;
};

// bview_listener_t
struct bview_listener_s {
    bview_listener_cb_t callback;
    void* udata;
    bview_listener_t* next;
    bview_listener_t* prev;
};

// cursor_t
struct cursor_s {
    bview_t* bview;
    mark_t* mark;
    mark_t* sel_bound;
    int is_sel_bound_anchored;
    int is_asleep;
    srule_t* sel_rule;
    char* cut_buffer;
    cursor_t* next;
    cursor_t* prev;
};

// kmacro_t
struct kmacro_s {
    char* name;
    kinput_t* inputs;
    size_t inputs_len;
    size_t inputs_cap;
    UT_hash_handle hh;
};

// cmd_funcref_t
struct cmd_funcref_s {
#define MLE_FUNCREF(pcmdfn) (cmd_funcref_t){ #pcmdfn, (pcmdfn), NULL }
#define MLE_FUNCREF_NONE (cmd_funcref_t){ NULL, NULL, NULL }
    char* name;
    cmd_func_t func;
    UT_hash_handle hh;
};

// kbinding_def_t
struct kbinding_def_s {
#define MLE_KBINDING_DEF(pcmdfn, pkey) { { #pcmdfn, (pcmdfn), NULL }, (pkey), NULL }
#define MLE_KBINDING_DEF_EX(pcmdfn, pkey, pstatic) { { #pcmdfn, (pcmdfn), NULL }, (pkey), (pstatic) }
    cmd_funcref_t funcref;
    char* key_patt;
    char* static_param;
};

// kbinding_t
struct kbinding_s {
    kinput_t input;
    cmd_funcref_t* funcref;
    char* static_param;
    kbinding_t* children;
    UT_hash_handle hh;
};

// kmap_node_t
struct kmap_node_s {
    kmap_t* kmap;
    bview_t* bview;
    kmap_node_t* next;
    kmap_node_t* prev;
};

// kmap_t
struct kmap_s {
    char* name;
    kbinding_t* bindings;
    int allow_fallthru;
    cmd_funcref_t* default_funcref;
    UT_hash_handle hh;
};

// cmd_context_t
struct cmd_context_s {
    #define MLE_PASTEBUF_INCR 1024
    editor_t* editor;
    bview_t* bview;
    cursor_t* cursor;
    kinput_t input;
    char* static_param;
    loop_context_t* loop_ctx;
    int is_user_input;
    kinput_t* pastebuf;
    size_t pastebuf_len;
    size_t pastebuf_size;
    int has_pastebuf_leftover;
    kinput_t pastebuf_leftover;
};

// loop_context_t
struct loop_context_s {
    #define MLE_LOOP_CTX_MAX_NUMERIC_LEN 20
    #define MLE_LOOP_CTX_MAX_NUMERIC_PARAMS 8
    #define MLE_LOOP_CTX_MAX_WILDCARD_PARAMS 8
    #define MLE_LOOP_CTX_MAX_COMPLETE_TERM_SIZE 256
    bview_t* invoker;
    char numeric[MLE_LOOP_CTX_MAX_NUMERIC_LEN + 1];
    kbinding_t* numeric_node;
    int numeric_len;
    uintmax_t numeric_params[MLE_LOOP_CTX_MAX_NUMERIC_PARAMS];
    int numeric_params_len;
    uint32_t wildcard_params[MLE_LOOP_CTX_MAX_WILDCARD_PARAMS];
    int wildcard_params_len;
    kbinding_t* binding_node;
    int need_more_input;
    int should_exit;
    char* prompt_answer;
    cmd_func_t prompt_callack;
    int tab_complete_index;
    char tab_complete_term[MLE_LOOP_CTX_MAX_COMPLETE_TERM_SIZE];
    cmd_funcref_t* last_cmd;
};

// async_proc_t
struct async_proc_s {
    editor_t* editor;
    bview_t* invoker;
    FILE* pipe;
    int pipefd;
    int is_done;
    struct timeval timeout;
    async_proc_cb_t callback;
    async_proc_t* next;
    async_proc_t* prev;
};

// editor functions
int editor_init(editor_t* editor, int argc, char** argv);
int editor_deinit(editor_t* editor);
int editor_run(editor_t* editor);
int editor_open_bview(editor_t* editor, bview_t* parent, int type, char* opt_path, int opt_path_len, int make_active, bint_t linenum, bview_rect_t* opt_rect, buffer_t* opt_buffer, bview_t** optret_bview);
int editor_close_bview(editor_t* editor, bview_t* bview, int* optret_num_closed);
int editor_set_active(editor_t* editor, bview_t* bview);
int editor_set_macro_toggle_key(editor_t* editor, char* key);
int editor_bview_exists(editor_t* editor, bview_t* bview);
int editor_bview_edit_count(editor_t* editor);
int editor_prompt(editor_t* editor, char* prompt, char* opt_data, int opt_data_len, kmap_t* opt_kmap, bview_listener_cb_t opt_cb, char** optret_answer);
int editor_menu(editor_t* editor, cmd_func_t callback, char* opt_buf_data, int opt_buf_data_len, async_proc_t* opt_aproc, bview_t** optret_menu);
int editor_prompt_menu(editor_t* editor, char* prompt, char* opt_buf_data, int opt_buf_data_len, bview_listener_cb_t opt_prompt_cb, async_proc_t* opt_aproc, char** optret_line);
int editor_count_bviews_by_buffer(editor_t* editor, buffer_t* buffer);
int editor_register_cmd(editor_t* editor, char* name, cmd_func_t opt_func, cmd_funcref_t** optret_funcref);

// bview functions
bview_t* bview_new(editor_t* editor, char* opt_path, int opt_path_len, buffer_t* opt_buffer);
int bview_open(bview_t* self, char* path, int path_len);
int bview_destroy(bview_t* self);
int bview_resize(bview_t* self, int x, int y, int w, int h);
int bview_draw(bview_t* self);
int bview_draw_cursor(bview_t* self, int set_real_cursor);
int bview_rectify_viewport(bview_t* self);
int bview_center_viewport_y(bview_t* self);
int bview_zero_viewport_y(bview_t* self);
int bview_push_kmap(bview_t* bview, kmap_t* kmap);
int bview_pop_kmap(bview_t* bview, kmap_t** optret_kmap);
int bview_split(bview_t* self, int is_vertical, float factor, bview_t** optret_bview);
int bview_unsplit(bview_t* parent, bview_t* child);
int bview_add_cursor(bview_t* self, bline_t* bline, bint_t col, cursor_t** optret_cursor);
int bview_remove_cursor(bview_t* self, cursor_t* cursor);
int bview_add_listener(bview_t* self, bview_listener_cb_t callback, void* udata);
int bview_set_syntax(bview_t* self, char* opt_syntax);
int bview_destroy_listener(bview_t* self, bview_listener_t* listener);
int bview_cursor_get_lo_hi(cursor_t* self, mark_t** ret_lo, mark_t** ret_hi);
bview_t* bview_get_split_root(bview_t* self);

// cmd functions
int cmd_insert_data(cmd_context_t* ctx);
int cmd_insert_newline(cmd_context_t* ctx);
int cmd_insert_tab(cmd_context_t* ctx);
int cmd_delete_before(cmd_context_t* ctx);
int cmd_delete_after(cmd_context_t* ctx);
int cmd_move_bol(cmd_context_t* ctx);
int cmd_move_eol(cmd_context_t* ctx);
int cmd_move_beginning(cmd_context_t* ctx);
int cmd_move_end(cmd_context_t* ctx);
int cmd_move_left(cmd_context_t* ctx);
int cmd_move_right(cmd_context_t* ctx);
int cmd_move_up(cmd_context_t* ctx);
int cmd_move_down(cmd_context_t* ctx);
int cmd_move_page_up(cmd_context_t* ctx);
int cmd_move_page_down(cmd_context_t* ctx);
int cmd_move_to_line(cmd_context_t* ctx);
int cmd_move_relative(cmd_context_t* ctx);
int cmd_move_word_forward(cmd_context_t* ctx);
int cmd_move_word_back(cmd_context_t* ctx);
int cmd_apply_macro(cmd_context_t* ctx);
int cmd_toggle_sel_bound(cmd_context_t* ctx);
int cmd_drop_sleeping_cursor(cmd_context_t* ctx);
int cmd_wake_sleeping_cursors(cmd_context_t* ctx);
int cmd_remove_extra_cursors(cmd_context_t* ctx);
int cmd_search(cmd_context_t* ctx);
int cmd_search_next(cmd_context_t* ctx);
int cmd_replace(cmd_context_t* ctx);
int cmd_redraw(cmd_context_t* ctx);
int cmd_find_word(cmd_context_t* ctx);
int cmd_isearch(cmd_context_t* ctx);
int cmd_delete_word_before(cmd_context_t* ctx);
int cmd_delete_word_after(cmd_context_t* ctx);
int cmd_cut(cmd_context_t* ctx);
int cmd_copy(cmd_context_t* ctx);
int cmd_uncut(cmd_context_t* ctx);
int cmd_next(cmd_context_t* ctx);
int cmd_prev(cmd_context_t* ctx);
int cmd_split_vertical(cmd_context_t* ctx);
int cmd_split_horizontal(cmd_context_t* ctx);
int cmd_close(cmd_context_t* ctx);
int cmd_save(cmd_context_t* ctx);
int cmd_save_as(cmd_context_t* ctx);
int cmd_open_new(cmd_context_t* ctx);
int cmd_open_file(cmd_context_t* ctx);
int cmd_open_replace_new(cmd_context_t* ctx);
int cmd_open_replace_file(cmd_context_t* ctx);
int cmd_fsearch(cmd_context_t* ctx);
int cmd_browse(cmd_context_t* ctx);
int cmd_grep(cmd_context_t* ctx);
int cmd_move_until_forward(cmd_context_t* ctx);
int cmd_move_until_back(cmd_context_t* ctx);
int cmd_copy_by(cmd_context_t* ctx);
int cmd_cut_by(cmd_context_t* ctx);
int cmd_apply_macro_by(cmd_context_t* ctx);
int cmd_undo(cmd_context_t* ctx);
int cmd_redo(cmd_context_t* ctx);
int cmd_quit(cmd_context_t* ctx);
int cmd_indent(cmd_context_t* ctx);
int cmd_outdent(cmd_context_t* ctx);
int cmd_shell(cmd_context_t* ctx);
int cmd_set_opt(cmd_context_t* ctx);
int cmd_drop_cursor_column(cmd_context_t* ctx);
int cmd_noop(cmd_context_t* ctx);

// async functions
async_proc_t* async_proc_new(bview_t* invoker, int timeout_sec, int timeout_usec, async_proc_cb_t callback, char* shell_cmd);
int async_proc_set_invoker(async_proc_t* aproc, bview_t* invoker);
int async_proc_destroy(async_proc_t* aproc);

// util functions
int util_shell_exec(editor_t* editor, char* cmd, long timeout_s, char* input, size_t input_len, char* opt_shell, char** ret_output, size_t* ret_output_len);
int util_popen2(char* cmd, char* opt_shell, int* ret_fdread, int* ret_fdwrite);
int util_get_bracket_pair(uint32_t ch, int* optret_is_closing);
int util_is_file(char* path, char* opt_mode, FILE** optret_file);
int util_is_dir(char* path);
int util_pcre_match(char* subject, char* re);
int util_timeval_is_gt(struct timeval* a, struct timeval* b);
char* util_escape_shell_arg(char* str, int len);
int tb_print(int x, int y, uint16_t fg, uint16_t bg, char *str);
int tb_printf(bview_rect_t rect, int x, int y, uint16_t fg, uint16_t bg, const char *fmt, ...);
int tb_printf_attr(bview_rect_t rect, int x, int y, const char *fmt, ...);

// Globals
extern editor_t _editor;

// Macros
#define MLE_VERSION "0.7"

#define MLE_OK 0
#define MLE_ERR 1

#define MLE_PROMPT_YES "yes"
#define MLE_PROMPT_NO "no"
#define MLE_PROMPT_ALL "all"

#define MLE_DEFAULT_TAB_WIDTH 4
#define MLE_DEFAULT_TAB_TO_SPACE 1
#define MLE_DEFAULT_MACRO_TOGGLE_KEY "M-r"

#define MLE_LOG_ERR(fmt, ...) do { \
    fprintf(stderr, (fmt), __VA_ARGS__); \
} while (0)

#define MLE_SET_ERR(editor, fmt, ...) do { \
    snprintf((editor)->errstr, MLE_ERRSTR_SIZE, (fmt), __VA_ARGS__); \
} while (0)

#define MLE_RETURN_ERR(editor, fmt, ...) do { \
    MLE_SET_ERR((editor), (fmt), __VA_ARGS__); \
    return MLE_ERR; \
} while (0)

#define MLE_MIN(a,b) (((a)<(b)) ? (a) : (b))
#define MLE_MAX(a,b) (((a)>(b)) ? (a) : (b))

#define MLE_BVIEW_IS_EDIT(bview) ((bview)->type == MLE_BVIEW_TYPE_EDIT)
#define MLE_BVIEW_IS_MENU(bview) ((bview)->is_menu && MLE_BVIEW_IS_EDIT(bview))
#define MLE_BVIEW_IS_POPUP(bview) ((bview)->type == MLE_BVIEW_TYPE_POPUP)
#define MLE_BVIEW_IS_STATUS(bview) ((bview)->type == MLE_BVIEW_TYPE_STATUS)
#define MLE_BVIEW_IS_PROMPT(bview) ((bview)->type == MLE_BVIEW_TYPE_PROMPT)

#define MLE_MARK_COL_TO_VCOL(pmark) ( \
    (pmark)->col >= (pmark)->bline->char_count \
    ? (pmark)->bline->char_vwidth \
    : ( (pmark)->col <= 0 ? 0 : (pmark)->bline->chars[(pmark)->col].vcol ) \
)

#define MLE_COL_TO_VCOL(pline, pcol, pmax) ( \
    (pcol) >= (pline)->char_count \
    ? (pmax) \
    : ( (pcol) <= 0 ? 0 : (pline)->chars[(pcol)].vcol ) \
)

// Sentinel values for numeric and wildcard kinputs
#define MLE_KINPUT_NUMERIC (kinput_t){ 0x40, 0xffffffff, 0xffff }
#define MLE_KINPUT_WILDCARD (kinput_t){ 0x80, 0xffffffff, 0xffff }

#define MLE_LINENUM_TYPE_ABS 0
#define MLE_LINENUM_TYPE_REL 1
#define MLE_LINENUM_TYPE_BOTH 2

#define MLE_PARAM_WILDCARD(pctx, pn) ( \
    (pn) < (pctx)->loop_ctx->wildcard_params_len \
    ? (pctx)->loop_ctx->wildcard_params[(pn)] \
    : 0 \
)

#define MLE_BRACKET_PAIR_MAX_SEARCH 10000

#define MLE_RE_WORD_FORWARD "((?<=\\w)\\W|$)"
#define MLE_RE_WORD_BACK "((?<=\\W)\\w|^)"


/*
TODO
[ ] options (e.g., tab size) associated with filetype
[ ] overlapping multi rules, range should be separate in styling
[ ] ensure multi_cursor_code impl for all appropriate
[ ] should not prompt for fname if present on exit
[ ] makefile params
[ ] option to ensure trailing newline
[ ] segfault hunt: splits
[ ] segfault hunt: async proc broken pipe
[ ] use MLE_RETURN_ERR more
[ ] --
[ ] experiment with adding ['>','<'] to brkt pairs for <C-d d>'ing html
[ ] aproc-bview refactor
[ ] factor out code into async_proc_read
[ ] large paste cuts off midway?
[ ] styles perf
[ ] perf with large files
[ ] drop/goto mark with char
[ ] add quote_pairs
[ ] async style refresh
[ ] find matching html bracket
[ ] last cmd status code indicator
[ ] srule priority / isearch hili in middle of multiline rule
[ ] slow indent
[ ] click to set cursor/focus
[ ] do not draw screen input continuous user inputs
[ ] cmd_replace back references
[ ] can't match ^$
[ ] flash messages "replaced N instances", "wrote N bytes"
[ ] scriptability + hooks
[ ] history per prompt?
[ ] cmd_var_set, _clear, _append, _prepend, _print, _incr, _decr
[ ] count allocs/frees, ensure 0 at exit
[ ] buffer_repeat
[ ] multi-level undo/redo
[ ] nested prompts?
[ ] bview_config_t
[ ] more elegant menu/prompt_menu code
[ ] command prompt
[ ] command wildcard param order
[ ] configurable colors
[ ] configurable status line
[ ] configurable caption line
[ ] guard against invalid api use, e.g., prevent srule_destroy on active srule

*/

#endif
