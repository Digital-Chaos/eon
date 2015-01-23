#include "mle.h"

#define MLE_MULTI_CURSOR_MARK_FN(pcursor, ptmp, pfn, ...) do { \
    DL_FOREACH((pcursor)->bview->cursors, (ptmp)) { \
        if ((ptmp)->is_asleep) continue; \
        pfn((ptmp)->mark, __VA_ARGS__); \
    } \
} while(0)

// Insert data
int cmd_insert_data(cmd_context_t* ctx) {
    char data[6];
    size_t data_len;
    cursor_t* cursor;
    data_len = utf8_unicode_to_char(data, ctx->input.ch);
    if (data_len < 1) {
        return MLE_OK;
    }
    MLE_MULTI_CURSOR_MARK_FN(ctx->cursor, cursor, mark_insert_before, data, data_len);
    return MLE_OK;
}

// Insert tab
int cmd_insert_tab(cmd_context_t* ctx) {
    int num_spaces;
    char* data;
    cursor_t* cursor;
    if (ctx->bview->tab_to_space) {
        num_spaces = ctx->bview->tab_size - (ctx->cursor->mark->col % ctx->bview->tab_size);
        data = malloc(num_spaces);
        memset(data, ' ', num_spaces);
        MLE_MULTI_CURSOR_MARK_FN(ctx->cursor, cursor, mark_insert_before, data, (size_t)num_spaces);
        free(data);
    } else {
        MLE_MULTI_CURSOR_MARK_FN(ctx->cursor, cursor, mark_insert_before, (char*)"\t", (size_t)1);
    }
    return MLE_OK;
}

int cmd_delete_before(cmd_context_t* ctx) {
    return MLE_OK;
}
int cmd_delete_after(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_bol(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_eol(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_beginning(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_end(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_left(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_right(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_up(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_down(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_page_up(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_page_down(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_to_line(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_word_forward(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_move_word_back(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_anchor_sel_bound(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_search(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_search_next(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_replace(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_isearch(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_delete_word_before(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_delete_word_after(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_cut(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_uncut(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_save(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_open(cmd_context_t* ctx) {
return MLE_OK; }
int cmd_quit(cmd_context_t* ctx) {
return MLE_OK; }
