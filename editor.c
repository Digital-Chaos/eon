#include <unistd.h>
#include <signal.h>
#include <termbox.h>
#include "uthash.h"
#include "utlist.h"
#include "mle.h"
#include "mlbuf.h"

static void _editor_startup(editor_t* editor);
static void _editor_loop(editor_t* editor, loop_context_t* loop_ctx);
static int _editor_maybe_toggle_macro(editor_t* editor, kinput_t* input);
static void _editor_resize(editor_t* editor, int w, int h);
static void _editor_display(editor_t* editor);
static void _editor_get_input(editor_t* editor, kinput_t* ret_input);
static void _editor_get_user_input(editor_t* editor, kinput_t* ret_input);
static void _editor_record_macro_input(editor_t* editor, kinput_t* input);
static cmd_function_t _editor_get_command(editor_t* editor, kinput_t input);
static int _editor_key_to_input(char* key, kinput_t* ret_input);
static void _editor_init_kmaps(editor_t* editor);
static void _editor_init_kmap(kmap_t** ret_kmap, char* name, cmd_function_t default_func, int allow_fallthru, kmap_def_t* defs);
static void _editor_init_syntaxes(editor_t* editor);
static void _editor_init_syntax(editor_t* editor, char* name, char* path_pattern, syntax_def_t* defs);
static void _editor_init_cli_args(editor_t* editor, int argc, char** argv);
static void _editor_init_status(editor_t* editor);
static void _editor_init_bviews(editor_t* editor, int argc, char** argv);
static int _editor_prompt_input_submit(cmd_context_t* ctx);
static int _editor_prompt_yn_yes(cmd_context_t* ctx);
static int _editor_prompt_yn_no(cmd_context_t* ctx);
static int _editor_prompt_cancel(cmd_context_t* ctx);
static int _editor_close_bview_inner(editor_t* editor, bview_t* bview);
static void _editor_destroy_kmap(kmap_t* kmap);
static void _editor_destroy_syntax_map(syntax_t* map);
static void _editor_draw_cursors(editor_t* editor, bview_t* bview);
static int _editor_bview_edit_count_inner(bview_t* bview);
static int _editor_bview_exists_inner(bview_t* parent, bview_t* needle);

// Init editor from args
int editor_init(editor_t* editor, int argc, char** argv) {
    // Set editor defaults
    editor->tab_width = MLE_DEFAULT_TAB_WIDTH;
    editor->tab_to_space = MLE_DEFAULT_TAB_TO_SPACE;
    editor->viewport_scope_x = -4;
    editor->viewport_scope_y = -4;
    editor->startup_linenum = -1;
    editor_set_macro_toggle_key(editor, MLE_DEFAULT_MACRO_TOGGLE_KEY);
    _editor_init_kmaps(editor);
    _editor_init_syntaxes(editor);

    // Parse cli args
    _editor_init_cli_args(editor, argc, argv);

    // Init status bar
    _editor_init_status(editor);

    // Init bviews
    _editor_init_bviews(editor, argc, argv);

    return MLE_OK;
}

// Run editor
int editor_run(editor_t* editor) {
    loop_context_t loop_ctx;
    loop_ctx.should_exit = 0;
    _editor_resize(editor, -1, -1);
    _editor_startup(editor);
    _editor_loop(editor, &loop_ctx);
    return MLE_OK;
}

// Deinit editor
int editor_deinit(editor_t* editor) {
    // TODO free stuff
    bview_t* bview;
    bview_t* tmp;
    bview_destroy(editor->status);
    DL_FOREACH_SAFE(editor->bviews, bview, tmp) {
        DL_DELETE(editor->bviews, bview);
        bview_destroy(bview);
    }
    _editor_destroy_kmap(editor->kmap_normal);
    _editor_destroy_kmap(editor->kmap_prompt_input);
    _editor_destroy_kmap(editor->kmap_prompt_yn);
    _editor_destroy_kmap(editor->kmap_prompt_ok);
    _editor_destroy_syntax_map(editor->syntax_map);
    return MLE_OK;
}

// Prompt user for input
int editor_prompt(editor_t* editor, char* prompt_key, char* label, char* opt_data, int opt_data_len, kmap_t* opt_kmap, char** optret_answer) {
    loop_context_t loop_ctx;

    // Disallow nested prompts
    // TODO nested prompts?
    if (editor->prompt) {
        if (optret_answer) *optret_answer = NULL;
        return MLE_ERR;
    }

    // Init loop_ctx
    loop_ctx.invoker = editor->active;
    loop_ctx.should_exit = 0;
    loop_ctx.prompt_answer = NULL;

    // Init prompt
    editor_open_bview(editor, MLE_BVIEW_TYPE_PROMPT, NULL, 0, 1, &editor->rect_prompt, NULL, &editor->prompt);
    editor->prompt->prompt_key = prompt_key;
    editor->prompt->prompt_label = label;
    bview_push_kmap(editor->prompt, opt_kmap ? opt_kmap : editor->kmap_prompt_input);

    // Insert opt_data if present
    if (opt_data && opt_data_len > 0) {
        buffer_insert(editor->prompt->buffer, 0, opt_data, opt_data_len, NULL);
        mark_move_eol(editor->prompt->active_cursor->mark);
    }

    // Loop inside prompt
    _editor_loop(editor, &loop_ctx);

    // Set answer
    if (optret_answer) {
        *optret_answer = loop_ctx.prompt_answer;
    } else if (loop_ctx.prompt_answer) {
        free(loop_ctx.prompt_answer);
        loop_ctx.prompt_answer = NULL;
    }

    // Restore previous focus
    editor_close_bview(editor, editor->prompt); // TODO nested prompts / editor->prompt as LL
    editor_set_active(editor, loop_ctx.invoker);
    editor->prompt = NULL;

    return MLE_OK;
}

// Open a bview
int editor_open_bview(editor_t* editor, int type, char* opt_path, int opt_path_len, int make_active, bview_rect_t* opt_rect, buffer_t* opt_buffer, bview_t** optret_bview) {
    bview_t* bview;
    bview = bview_new(editor, opt_path, opt_path_len, opt_buffer);
    bview->type = type;
    DL_APPEND(editor->bviews, bview);
    editor->bviews_tail = bview;
    if (make_active) {
        editor_set_active(editor, bview);
    }
    if (opt_rect) {
        bview_resize(bview, opt_rect->x, opt_rect->y, opt_rect->w, opt_rect->h);
    }
    if (optret_bview) {
        *optret_bview = bview;
    }
    return MLE_OK;
}

// Close a bview
int editor_close_bview(editor_t* editor, bview_t* bview) {
    int rc;
    if ((rc = _editor_close_bview_inner(editor, bview)) == MLE_OK) {
        _editor_resize(editor, editor->w, editor->h);
    }
    return rc;
}

// Set the active bview
int editor_set_active(editor_t* editor, bview_t* bview) {
    if (!editor_bview_exists(editor, bview)) {
        MLE_RETURN_ERR("No bview %p in editor->bviews\n", bview);
    }
    editor->active = bview;
    if (MLE_BVIEW_IS_EDIT(bview)) {
        editor->active_edit = bview;
        editor->active_edit_root = bview_get_split_root(bview);
    }
    bview_rectify_viewport(bview);
    return MLE_OK;
}

// Set macro toggle key
int editor_set_macro_toggle_key(editor_t* editor, char* key) {
    return _editor_key_to_input(key, &editor->macro_toggle_key);
}

// Return 1 if bview exists in editor, else return 0
int editor_bview_exists(editor_t* editor, bview_t* bview) {
    bview_t* tmp;
    DL_FOREACH(editor->bviews, tmp) {
        if (_editor_bview_exists_inner(tmp, bview)) {
            return 1;
        }
    }
    raise(SIGINT);
    return 0;
}

// Return number of EDIT bviews open
int editor_bview_edit_count(editor_t* editor) {
    int count;
    bview_t* bview;
    count = 0;
    DL_FOREACH(editor->bviews, bview) {
        if (MLE_BVIEW_IS_EDIT(bview)) {
            count += _editor_bview_edit_count_inner(bview);
        }
    }
    return count;
}

// Return 1 if parent or a child of parent == needle, otherwise return 0
static int _editor_bview_exists_inner(bview_t* parent, bview_t* needle) {
    if (parent == needle) {
        return 1;
    }
    if (parent->split_child) {
        return _editor_bview_exists_inner(parent->split_child, needle);
    }
    return 0;
}

// Return number of edit bviews open including split children
static int _editor_bview_edit_count_inner(bview_t* bview) {
    if (bview->split_child) {
        return 1 + _editor_bview_edit_count_inner(bview->split_child);
    }
    return 1;
}

// Close a bview
static int _editor_close_bview_inner(editor_t* editor, bview_t* bview) {
    if (!editor_bview_exists(editor, bview)) {
        MLE_RETURN_ERR("No bview %p in editor->bviews\n", bview);
    }
    if (bview->split_child) {
        _editor_close_bview_inner(editor, bview->split_child);
    }
    DL_DELETE(editor->bviews, bview);
    if (bview->split_parent) {
        bview->split_parent->split_child = NULL;
        editor_set_active(editor, bview->split_parent);
    } else { 
        if (bview->prev && bview->prev != bview && MLE_BVIEW_IS_EDIT(bview->prev)) {
            editor_set_active(editor, bview->prev);
        } else if (bview->next && MLE_BVIEW_IS_EDIT(bview->next)) {
            editor_set_active(editor, bview->next);
        } else {
            editor_open_bview(editor, MLE_BVIEW_TYPE_EDIT, NULL, 0, 1, &editor->rect_edit, NULL, NULL);
        }
    }
    bview_destroy(bview);
    return MLE_OK;
}

// Invoked when user hits enter in a prompt_input
static int _editor_prompt_input_submit(cmd_context_t* ctx) {
    bint_t answer_len;
    char* answer;
    buffer_get(ctx->bview->buffer, &answer, &answer_len);
    ctx->loop_ctx->prompt_answer = strndup(answer, answer_len);
    ctx->loop_ctx->should_exit = 1;
    return MLE_OK;
}

// Invoked when user hits y in a prompt_yn
static int _editor_prompt_yn_yes(cmd_context_t* ctx) {
    ctx->loop_ctx->prompt_answer = MLE_PROMPT_YES;
    ctx->loop_ctx->should_exit = 1;
    return MLE_OK;
}

// Invoked when user hits n in a prompt_yn
static int _editor_prompt_yn_no(cmd_context_t* ctx) {
    ctx->loop_ctx->prompt_answer = MLE_PROMPT_NO;
    ctx->loop_ctx->should_exit = 1;
    return MLE_OK;
}

// Invoked when user cancels (Ctrl-C) a prompt_(input|yn), or hits any key in a prompt_ok
static int _editor_prompt_cancel(cmd_context_t* ctx) {
    ctx->loop_ctx->prompt_answer = NULL;
    ctx->loop_ctx->should_exit = 1;
    return MLE_OK;
}

// Run startup actions. This is before any user-input is processed.
static void _editor_startup(editor_t* editor) {
    // Jump to line in current bview if specified
    if (editor->startup_linenum >= 0) {
        mark_move_to(editor->active_edit->active_cursor->mark, editor->startup_linenum, 0);
        bview_center_viewport_y(editor->active_edit);
    }
}

// Run editor loop
static void _editor_loop(editor_t* editor, loop_context_t* loop_ctx) {
    cmd_context_t cmd_ctx;
    cmd_function_t cmd_fn;

    // Init cmd_context
    memset(&cmd_ctx, 0, sizeof(cmd_context_t));
    cmd_ctx.editor = editor;
    cmd_ctx.loop_ctx = loop_ctx;

    // Loop until editor should exit
    while (!loop_ctx->should_exit) {

        // Display editor
        if (!editor->is_display_disabled) {
            _editor_display(editor);
        }

        // Get input
        _editor_get_input(editor, &cmd_ctx.input);

        // Toggle macro?
        if (_editor_maybe_toggle_macro(editor, &cmd_ctx.input) == 1) {
            continue;
        }

        // Execute command
        if ((cmd_fn = _editor_get_command(editor, cmd_ctx.input)) != NULL) {
            cmd_ctx.cursor = editor->active ? editor->active->active_cursor : NULL;
            cmd_ctx.bview = cmd_ctx.cursor ? cmd_ctx.cursor->bview : NULL;
            cmd_fn(&cmd_ctx);
        } else {
            // TODO log error
        }
    }
}

// If input == editor->macro_toggle_key, toggle macro mode and return 1
static int _editor_maybe_toggle_macro(editor_t* editor, kinput_t* input) {
    if (memcmp(input, &editor->macro_toggle_key, sizeof(kinput_t)) == 0) {
        if (editor->is_recording_macro) {
            // Stop recording macro and add to map
            HASH_ADD(hh, editor->macro_map, key, sizeof(kinput_t), editor->macro_record);
            editor->macro_record = NULL;
            editor->is_recording_macro = 0;
        } else {
            // Get macro_key and start recording
            editor->macro_record = calloc(1, sizeof(kmacro_t));
            _editor_get_input(editor, &editor->macro_record->key);
            editor->is_recording_macro = 1;
        }
        return 1;
    }
    return 0;
}

// Resize the editor
static void _editor_resize(editor_t* editor, int w, int h) {
    bview_t* bview;
    bview_rect_t* bounds;

    editor->w = w >= 0 ? w : tb_width();
    editor->h = h >= 0 ? h : tb_height();

    editor->rect_edit.x = 0;
    editor->rect_edit.y = 0;
    editor->rect_edit.w = editor->w;
    editor->rect_edit.h = editor->h - 2;

    editor->rect_status.x = 0;
    editor->rect_status.y = editor->h - 2;
    editor->rect_status.w = editor->w;
    editor->rect_status.h = 1;

    editor->rect_prompt.x = 0;
    editor->rect_prompt.y = editor->h - 1;
    editor->rect_prompt.w = editor->w;
    editor->rect_prompt.h = 1;

    editor->rect_popup.x = 0;
    editor->rect_popup.y = editor->h - 2 - (editor->popup_h);
    editor->rect_popup.w = editor->w;
    editor->rect_popup.h = editor->popup_h;

    DL_FOREACH(editor->bviews, bview) {
        if (MLE_BVIEW_IS_PROMPT(bview)) {
            bounds = &editor->rect_prompt;
        } else if (MLE_BVIEW_IS_POPUP(bview)) {
            bounds = &editor->rect_popup;
        } else if (MLE_BVIEW_IS_STATUS(bview)) {
            bounds = &editor->rect_status;
        } else {
            if (bview->split_parent) continue;
            bounds = &editor->rect_edit;
        }
        bview_resize(bview, bounds->x, bounds->y, bounds->w, bounds->h);
    }
}

// Draw bviews cursors recursively
static void _editor_draw_cursors(editor_t* editor, bview_t* bview) {
    if (MLE_BVIEW_IS_EDIT(bview) && bview_get_split_root(bview) != editor->active_edit_root) {
        return;
    }
    bview_draw_cursor(bview, bview == editor->active ? 1 : 0);
    if (bview->split_child) {
        _editor_draw_cursors(editor, bview->split_child);
    }
}

// Display the editor
static void _editor_display(editor_t* editor) {
    bview_t* bview;
    tb_clear();
    bview_draw(editor->active_edit_root);
    bview_draw(editor->status);
    if (editor->popup) bview_draw(editor->popup);
    if (editor->prompt) bview_draw(editor->prompt);
    DL_FOREACH(editor->bviews, bview) _editor_draw_cursors(editor, bview);
    tb_present();
}

// Get input from either macro or user
static void _editor_get_input(editor_t* editor, kinput_t* ret_input) {
    while (1) {
        if (editor->macro_apply
            && editor->macro_apply_input_index < editor->macro_apply->inputs_len
        ) {
            // Get input from macro
            *ret_input = editor->macro_apply->inputs[editor->macro_apply_input_index];
            editor->macro_apply_input_index += 1;
        } else {
            // Clear macro
            if (editor->macro_apply) {
                editor->macro_apply = NULL;
                editor->macro_apply_input_index = 0;
            }
            // Get user input
            _editor_get_user_input(editor, ret_input);
        }
        if (editor->is_recording_macro) {
            // Record macro input
            _editor_record_macro_input(editor, ret_input);
        }
        break;
    }
}

// Get user input
static void _editor_get_user_input(editor_t* editor, kinput_t* ret_input) {
    int rc;
    tb_event_t ev;
    while (1) {
        rc = tb_poll_event(&ev);
        if (rc == -1) {
            continue;
        } else if (rc == TB_EVENT_RESIZE) {
            _editor_resize(editor, ev.w, ev.h);
            _editor_display(editor);
            continue;
        }
        *ret_input = (kinput_t){ ev.mod, ev.ch, ev.key };
        break;
    }
}

// Copy input into macro buffer
static void _editor_record_macro_input(editor_t* editor, kinput_t* input) {
    kmacro_t* macro;
    macro = editor->macro_record;
    if (!macro->inputs) {
        macro->inputs = calloc(8, sizeof(kinput_t));
        macro->inputs_len = 0;
        macro->inputs_cap = 8;
    } else if (macro->inputs_len + 1 > macro->inputs_cap) {
        macro->inputs_cap = macro->inputs_len + 8;
        macro->inputs = realloc(macro->inputs, macro->inputs_cap * sizeof(kinput_t));
    }
    memcpy(macro->inputs + macro->inputs_len, input, sizeof(kinput_t));
    macro->inputs_len += 1;
}

// Return command for input
static cmd_function_t _editor_get_command(editor_t* editor, kinput_t input) {
    kmap_node_t* kmap_node;
    kbinding_t tmp_binding;
    kbinding_t* binding;

    kmap_node = editor->active->kmap_tail;
    memset(&tmp_binding, 0, sizeof(kbinding_t));
    tmp_binding.input = input;
    binding = NULL;
    while (kmap_node) {
        HASH_FIND(hh, kmap_node->kmap->bindings, &tmp_binding.input, sizeof(kinput_t), binding);
        if (binding) {
            return binding->func;
        } else if (kmap_node->kmap->default_func) {
            return kmap_node->kmap->default_func;
        }
        if (kmap_node->kmap->allow_fallthru) {
            kmap_node = kmap_node->prev;
        } else {
            kmap_node = NULL;
        }
    }
    return NULL;
}

// Return a kinput_t given a key name
static int _editor_key_to_input(char* key, kinput_t* ret_input) {
    int keylen;
    int mod;
    uint32_t ch;
    keylen = strlen(key);

    // Check for special key
#define MLE_KEY_DEF(pckey, pmod, pch, pkey) \
    } else if (!strncmp((pckey), key, keylen)) { \
        *ret_input = (kinput_t){ (pmod), 0, (pkey) }; \
        return MLE_OK;
    if (keylen < 1) {
        MLE_RETURN_ERR("key has length %d\n", keylen);
#include "keys.h"
    }
#undef MLE_KEY_DEF

    // Check for character, with potential ALT modifier
    mod = 0;
    ch = 0;
    if (keylen > 2 && !strncmp("M-", key, 2)) {
        mod = TB_MOD_ALT;
        key += 2;
    }
    utf8_char_to_unicode(&ch, key, NULL);
    if (ch < 1) {
        return MLE_ERR;
    }
    *ret_input = (kinput_t){ mod, ch, 0 };
    return MLE_OK;
}

// Init built-in kmaps
static void _editor_init_kmaps(editor_t* editor) {
    _editor_init_kmap(&editor->kmap_normal, "normal", cmd_insert_data, 0, (kmap_def_t[]){
        { cmd_insert_tab, "tab" },
        { cmd_insert_newline, "enter", },
        { cmd_delete_before, "backspace" },
        { cmd_delete_before, "backspace2" },
        { cmd_delete_after, "delete" },
        { cmd_move_bol, "C-a" },
        { cmd_move_bol, "home" },
        { cmd_move_eol, "C-e" },
        { cmd_move_eol, "end" },
        { cmd_move_beginning, "M-\\" },
        { cmd_move_end, "M-/" },
        { cmd_move_left, "left" },
        { cmd_move_right, "right" },
        { cmd_move_up, "up" },
        { cmd_move_down, "down" },
        { cmd_move_page_up, "page-up" },
        { cmd_move_page_down, "page-down" },
        { cmd_move_to_line, "M-g" },
        { cmd_move_word_forward, "M-f" },
        { cmd_move_word_back, "M-b" },
        { cmd_toggle_sel_bound, "M-a" },
        { cmd_drop_sleeping_cursor, "M-h" },
        { cmd_wake_sleeping_cursors, "M-j" },
        { cmd_remove_extra_cursors, "M-k" },
        { cmd_search, "C-f" },
        { cmd_search_next, "C-j" },
        { cmd_replace, "M-r" },
        { cmd_isearch, "C-r" },
        { cmd_delete_word_before, "C-w" },
        { cmd_delete_word_after, "M-d" },
        { cmd_cut, "C-k" },
        { cmd_uncut, "C-u" },
        { cmd_next, "M-n" },
        { cmd_prev, "M-p" },
        { cmd_split_vertical, "M-l" },
        { cmd_split_horizontal, "M-;" },
        { cmd_save, "C-o" },
        { cmd_new, "C-n" },
        { cmd_new_open, "C-b" },
        { cmd_replace_new, "C-p" },
        { cmd_replace_open, "C-l" },
        { cmd_close, "M-c" },
        { cmd_quit, "C-q" },
        { NULL, "" }
    });
    _editor_init_kmap(&editor->kmap_prompt_input, "prompt_input", NULL, 1, (kmap_def_t[]){
        { _editor_prompt_input_submit, "enter" },
        { _editor_prompt_cancel, "C-c" },
        { NULL, "" }
    });
    _editor_init_kmap(&editor->kmap_prompt_yn, "prompt_yn", NULL, 0, (kmap_def_t[]){
        { _editor_prompt_yn_yes, "y" },
        { _editor_prompt_yn_no, "n" },
        { _editor_prompt_cancel, "C-c" },
        { NULL, "" }
    });
    _editor_init_kmap(&editor->kmap_prompt_ok, "prompt_ok", _editor_prompt_cancel, 0, (kmap_def_t[]){
        { NULL, "" }
    });
    // TODO
}

// Init a single kmap
static void _editor_init_kmap(kmap_t** ret_kmap, char* name, cmd_function_t default_func, int allow_fallthru, kmap_def_t* defs) {
    kmap_t* kmap;
    kbinding_t* binding;

    kmap = calloc(1, sizeof(kmap_t));
    snprintf(kmap->name, MLE_KMAP_NAME_MAX_LEN, "%s", name);
    kmap->allow_fallthru = allow_fallthru;
    kmap->default_func = default_func;

    while (defs && defs->func) {
        binding = calloc(1, sizeof(kbinding_t));
        binding->func = defs->func;
        if (_editor_key_to_input(defs->key, &binding->input) != MLE_OK) {
            // TODO log error
            free(binding);
            defs++;
            continue;
        }
        HASH_ADD(hh, kmap->bindings, input, sizeof(kinput_t), binding);
        defs++;
    }

    *ret_kmap = kmap;
}

// Destroy a kmap
static void _editor_destroy_kmap(kmap_t* kmap) {
    kbinding_t* binding;
    kbinding_t* binding_tmp;
    HASH_ITER(hh, kmap->bindings, binding, binding_tmp) {
        HASH_DELETE(hh, kmap->bindings, binding);
        free(binding);
    }
    free(kmap);
}

// Init built-in syntax map
static void _editor_init_syntaxes(editor_t* editor) {
    _editor_init_syntax(editor, "generic", "\\.(c|cpp|h|hpp|php|py|rb|sh|pl|go|js|java|lua)$", (syntax_def_t[]){
        { "(?<![\\w%@$])("
          "abstract|alias|alignas|alignof|and|and_eq|arguments|array|as|asm|"
          "assert|auto|base|begin|bitand|bitor|bool|boolean|break|byte|"
          "callable|case|catch|chan|char|checked|class|clone|cmp|compl|const|"
          "const_cast|constexpr|continue|debugger|decimal|declare|decltype|"
          "def|default|defer|defined|del|delegate|delete|die|do|done|double|"
          "dynamic_cast|echo|elif|else|elseif|elsif|empty|end|enddeclare|"
          "endfor|endforeach|endif|endswitch|endwhile|ensure|enum|eq|esac|"
          "eval|event|except|exec|exit|exp|explicit|export|extends|extern|"
          "fallthrough|false|fi|final|finally|fixed|float|for|foreach|friend|"
          "from|func|function|ge|global|go|goto|gt|if|implements|implicit|"
          "import|in|include|include_once|inline|instanceof|insteadof|int|"
          "interface|internal|is|isset|lambda|le|let|list|lock|long|lt|m|map|"
          "module|mutable|namespace|native|ne|new|next|nil|no|noexcept|not|"
          "not_eq|null|nullptr|object|operator|or|or_eq|out|override|package|"
          "params|pass|print|private|protected|public|q|qq|qr|qw|qx|raise|"
          "range|readonly|redo|ref|register|reinterpret_cast|require|"
          "require_once|rescue|retry|return|s|sbyte|sealed|select|self|short|"
          "signed|sizeof|stackalloc|static|static_assert|static_cast|"
          "strictfp|string|struct|sub|super|switch|synchronized|template|"
          "then|this|thread_local|throw|throws|time|tr|trait|transient|true|"
          "try|type|typedef|typeid|typename|typeof|uint|ulong|unchecked|"
          "undef|union|unless|unsafe|unset|unsigned|until|use|ushort|using|"
          "var|virtual|void|volatile|when|while|with|xor|xor_eq|y|yield"
          ")\\b", NULL, TB_GREEN, TB_DEFAULT },
        { "[(){}<>\\[\\].,;:?!+=/\\\\%^*-]", NULL, TB_RED | TB_BOLD, TB_DEFAULT },
        { "(?<!\\w)[\\%@$][a-zA-Z_$][a-zA-Z0-9_]*\\b", NULL, TB_GREEN, TB_DEFAULT },
        { "\\b[A-Z_][A-Z0-9_]*\\b", NULL, TB_RED | TB_BOLD, TB_DEFAULT },
        { "\\b(-?(0x)?[0-9]+|true|false|null)\\b", NULL, TB_BLUE | TB_BOLD, TB_DEFAULT },
        { "'([^']|\\')*'", NULL, TB_YELLOW | TB_BOLD, TB_DEFAULT },
        { "\"([^\"]|\\\")*\"", NULL, TB_YELLOW | TB_BOLD, TB_DEFAULT },
        { "/([^/]|/)*" "/", NULL, TB_YELLOW, TB_DEFAULT },
        { "/" "/.*$", NULL, TB_CYAN, TB_DEFAULT },
        { "/\\" "*", "\\*" "/", TB_CYAN, TB_DEFAULT },
        { "<\\?(php)?|\\?>", NULL, TB_GREEN, TB_DEFAULT },
        { "\\?>", "<\\?(php)?", TB_WHITE, TB_DEFAULT },
        { "\"\"\"", "\"\"\"", TB_YELLOW | TB_BOLD, TB_DEFAULT },
        { "\\t+", NULL, TB_DEFAULT, TB_RED },
        { "\\s+$", NULL, TB_DEFAULT, TB_GREEN },
        { NULL, NULL, 0, 0 }
    });
}

// Init a single syntax
static void _editor_init_syntax(editor_t* editor, char* name, char* path_pattern, syntax_def_t* defs) {
    syntax_t* syntax;
    srule_node_t* node;

    syntax = calloc(1, sizeof(syntax_t));
    snprintf(syntax->name, MLE_SYNTAX_NAME_MAX_LEN, "%s", name);
    syntax->path_pattern = path_pattern;

    while (defs && defs->re) {
        node = calloc(1, sizeof(srule_node_t));
        if (defs->re_end) {
            node->srule = srule_new_multi(defs->re, strlen(defs->re), defs->re_end, strlen(defs->re_end), defs->fg, defs->bg);
        } else {
            node->srule = srule_new_single(defs->re, strlen(defs->re), defs->fg, defs->bg);
        }
        DL_APPEND(syntax->srules, node);
        defs++;
    }

    HASH_ADD_STR(editor->syntax_map, name, syntax);
}

// Destroy a syntax
static void _editor_destroy_syntax_map(syntax_t* map) {
    syntax_t* syntax;
    syntax_t* syntax_tmp;
    srule_node_t* srule;
    srule_node_t* srule_tmp;
    HASH_ITER(hh, map, syntax, syntax_tmp) {
        HASH_DELETE(hh, map, syntax);
        DL_FOREACH_SAFE(syntax->srules, srule, srule_tmp) {
            DL_DELETE(syntax->srules, srule);
            srule_destroy(srule->srule);
            free(srule);
        }
        free(syntax);
    }
}

// Parse cli args
static void _editor_init_cli_args(editor_t* editor, int argc, char** argv) {
    int c;
    while ((c = getopt(argc, argv, "hAms:t:v")) != -1) {
        switch (c) {
            case 'h':
                printf("mle version %s\n\n", MLE_VERSION);
                printf("Usage: mle [options] [file]...\n\n");
                printf("    -A           Allow tabs (disable tab-to-space)\n");
                printf("    -h           Show this message\n");
                printf("    -m <key>     Set macro toggle key (default: C-x)\n");
                printf("    -s <syntax>  Specify override syntax\n");
                printf("    -t <size>    Set tab size (default: %d)\n", MLE_DEFAULT_TAB_WIDTH);
                printf("    -v           Print version and exit\n");
                printf("    file         At start up, open file\n");
                printf("    file:line    At start up, open file at line\n");
                exit(EXIT_SUCCESS);
            case 'A':
                editor->tab_to_space = 0;
                break;
            case 'm':
                if (editor_set_macro_toggle_key(editor, optarg) != MLE_OK) {
                    MLE_LOG_ERR("Could not set macro key to %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case 's':
                editor->syntax_override = optarg;
                break;
            case 't':
                editor->tab_width = atoi(optarg);
                break;
            case 'v':
                printf("mle version %s\n", MLE_VERSION);
                exit(EXIT_SUCCESS);
        }
    }
}

// Init status bar
static void _editor_init_status(editor_t* editor) {
    editor->status = bview_new(editor, NULL, 0, NULL);
    editor->status->type = MLE_BVIEW_TYPE_STATUS;
    editor->rect_status.fg = TB_WHITE;
    editor->rect_status.bg = TB_BLACK | TB_BOLD;
}

// Init bviews
static void _editor_init_bviews(editor_t* editor, int argc, char** argv) {
    int i;
    char* colon;
    bview_t* bview;
    char *path;
    int path_len;

    // Open bviews
    if (optind >= argc) {
        // Open blank
        editor_open_bview(editor, MLE_BVIEW_TYPE_EDIT, NULL, 0, 1, &editor->rect_edit, NULL, NULL);
    } else {
        // Open files
        for (i = optind; i < argc; i++) {
            path = argv[i];
            path_len = strlen(argv[i]);
            if (util_file_exists(path, path_len)) {
                editor_open_bview(editor, MLE_BVIEW_TYPE_EDIT, path, path_len, 1, &editor->rect_edit, NULL, NULL);
            } else if ((colon = strrchr(path, ':')) != NULL) {
                path_len = colon - path;
                editor->startup_linenum = strtoul(colon + 1, NULL, 10);
                if (editor->startup_linenum > 0) editor->startup_linenum -= 1;
                editor_open_bview(editor, MLE_BVIEW_TYPE_EDIT, path, path_len, 1, &editor->rect_edit, NULL, &bview);
            } else {
                editor_open_bview(editor, MLE_BVIEW_TYPE_EDIT, path, path_len, 1, &editor->rect_edit, NULL, NULL);
            }
        }
    }
}
