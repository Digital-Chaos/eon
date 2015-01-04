#include <math.h>
#include "mle.h"

static void _bview_init(bview_t* self, buffer_t* buffer);
static void _bview_deinit(bview_t* self);
static buffer_t* _bview_open_buffer(char* path, int path_len);

// Create a new bview
bview_t* bview_new(editor_t* editor, char* opt_path, int opt_path_len, buffer_t* opt_buffer) {
    bview_t* self;
    buffer_t* buffer;

    // Open buffer
    if (opt_buffer) {
        buffer = opt_buffer;
    } else {
        buffer = _bview_open_buffer(opt_path, opt_path_len);
    }

    // Allocate and init bview
    self = calloc(1, sizeof(bview_t));
    self->editor = editor;
    self->path = strndup(opt_path, opt_path_len);
    self->rect_caption.bg = TB_REVERSE;
    self->rect_lines.fg = TB_YELLOW;
    self->rect_margin_left.fg = TB_RED;
    self->rect_margin_right.fg = TB_RED;
    self->tab_size = editor->tab_size;
    self->tab_to_space = editor->tab_to_space;
    self->viewport_scope_x = editor->viewport_scope_x;
    self->viewport_scope_y = editor->viewport_scope_y;
    _bview_init(self, buffer);

    return self;
}

// Open a buffer in an existing bview
int bview_open(bview_t* self, char* path, int path_len) {
    buffer_t* buffer;
    buffer = _bview_open_buffer(path, path_len);
    if (self->path) free(self->path);
    self->path = strndup(path, path_len);
    _bview_init(self, buffer);
    return MLE_OK;
}

// Free a bview
int bview_destroy(bview_t* self) {
    _bview_deinit(self);
    if (self->path) free(self->path);
    // TODO ensure everything freed
    free(self);
    return MLE_OK;
}

// Move and resize a bview to the given position and dimensions
int bview_resize(bview_t* self, int x, int y, int w, int h) {
    int aw, ah;

    aw = w;
    ah = h;

    if (self->split_child) {
        if (self->split_is_vertical) {
            aw = MLE_MAX(1, (int)((float)aw * self->split_factor));
        } else {
            ah = MLE_MAX(1, (int)((float)ah * self->split_factor));
        }
    }

    self->x = x;
    self->y = y;
    self->w = aw;
    self->h = ah;

    if (MLE_BVIEW_IS_EDIT(self)) {
        self->rect_caption.x = x;
        self->rect_caption.y = y;
        self->rect_caption.w = aw;
        self->rect_caption.h = 1;

        self->rect_lines.x = x;
        self->rect_lines.y = y + 1;
        self->rect_lines.w = self->line_num_width;
        self->rect_lines.h = ah - 1;

        self->rect_margin_left.x = x + self->line_num_width;
        self->rect_margin_left.y = y + 1;
        self->rect_margin_left.w = 1;
        self->rect_margin_left.h = ah - 1;

        self->rect_buffer.x = x + self->line_num_width + 1;
        self->rect_buffer.y = y + 1;
        self->rect_buffer.w = aw - (self->line_num_width + 1 + 1);
        self->rect_buffer.h = ah - 1;

        self->rect_margin_right.x = x + (aw - 1);
        self->rect_margin_right.y = y + 1;
        self->rect_margin_right.w = 1;
        self->rect_margin_right.h = ah - 1;
    } else {
        self->rect_buffer.x = x;
        self->rect_buffer.y = y;
        self->rect_buffer.w = aw;
        self->rect_buffer.h = ah;
    }

    if (self->split_child) {
        bview_resize(
            self->split_child,
            x + (self->split_is_vertical ? aw : 0),
            y + (self->split_is_vertical ? 0 : ah),
            w - (self->split_is_vertical ? aw : 0),
            h - (self->split_is_vertical ? 0 : ah)
        );
    }

    return MLE_OK;
}

// Draw bview to screen
int bview_draw(bview_t* self) {
    // TODO
}

int bview_push_kmap(bview_t* bview, kmap_t* kmap) {
    // TODO
}

int bview_pop_kmap(bview_t* bview, kmap_t** optret_kmap) {
    // TODO
}

// Split a bview
int bview_split(bview_t* self, int is_vertical, float factor, bview_t** optret_bview) {
    bview_t* child;

    if (self->split_child) {
        MLE_RETURN_ERR("bview %p is already split\n", self);
    } else if (!MLE_BVIEW_IS_EDIT(self)) {
        MLE_RETURN_ERR("bview %p is not an edit bview\n", self);
    }

    // Make child
    editor_open_bview(self->editor, NULL, 0, 1, self->buffer, &child);
    self->split_child = child;
    self->split_factor = factor;
    self->split_is_vertical = is_vertical;
    // TODO copy viewport and cursor position

    // Resize self
    bview_resize(self, self->x, self->y, self->w, self->h);

    if (optret_bview) {
        *optret_bview = child;
    }
    return MLE_OK;
}

// Unsplit a bview
int bview_unsplit(bview_t* self) {
    if (!self->split_child) {
        MLE_RETURN_ERR("bview %p is not split\n", self);
    }

    // Unsplit children first
    if (self->split_child->split_child) {
        bview_unsplit(self->split_child);
    }

    // Close child
    editor_close_bview(self->editor, self->split_child);
    self->split_child = NULL;
    self->split_factor = (float)0;
    self->split_is_vertical = 0;

    // Resize self
    bview_resize(self, self->x, self->y, self->w, self->h);

    return MLE_OK;
}

// Add a cursor to a bview
int bview_add_cursor(bview_t* self, bline_t* bline, size_t col, cursor_t** optret_cursor) {
    cursor_t* cursor;
    cursor = calloc(1, sizeof(cursor_t));
    cursor->bview = self;
    cursor->mark = buffer_add_mark(self->buffer, bline, col);
    DL_APPEND(self->cursors, cursor);
    if (!self->active_cursor) self->active_cursor = cursor;
    if (optret_cursor) {
        *optret_cursor = cursor;
    }
    return MLE_OK;
}

// Remove a cursor from a bview
int bview_remove_cursor(bview_t* self, cursor_t* cursor) {
    cursor_t* el;
    cursor_t* tmp;
    DL_FOREACH_SAFE(self->cursors, el, tmp) {
        if (el == cursor) {
            self->active_cursor = el->prev ? el->prev : el->next;
            DL_DELETE(self->cursors, el);
            free(el);
            return MLE_OK;
        }
    }
    return MLE_ERR;
}

// Init a bview with a buffer
static void _bview_init(bview_t* self, buffer_t* buffer) {
    cursor_t* cursor_tmp;

    _bview_deinit(self);

    // Reference buffer
    self->buffer = buffer;
    self->buffer->ref_count += 1;
    self->line_num_width = MLE_MAX(1, (int)log10((double)self->buffer->line_count));

    // Push normal mode
    bview_push_kmap(self, self->editor->kmap_normal);

    // Add a cursor
    bview_add_cursor(self, self->buffer->first_line, 0, &cursor_tmp);
}

// Deinit a bview
static void _bview_deinit(bview_t* self) {
    // Dereference/free buffer
    if (self->buffer) {
        self->buffer->ref_count -= 1;
        if (self->buffer->ref_count < 1) {
            buffer_destroy(self->buffer);
        }
    }

    // Remove all kmaps
    while (self->kmap_tail) {
        bview_pop_kmap(self, NULL);
    }

    // Remove all cursors
    while (self->active_cursor) {
        bview_remove_cursor(self, self->active_cursor);
    }
}

// Open a buffer with an optional path to load, otherwise empty
static buffer_t* _bview_open_buffer(char* opt_path, int opt_path_len) {
    buffer_t* buffer;
    buffer = NULL;
    if (opt_path && opt_path_len > 0) {
        buffer = buffer_new_open(opt_path, opt_path_len);
    }
    if (!buffer) {
        buffer = buffer_new();
    }
    return buffer;
}
