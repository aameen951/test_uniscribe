#include "my_std.h"
#include "resizable_buffer.h"

struct FontData {
  int line_height;
  wchar_t *font_name;
  HFONT font;
  // script cache is per font. It is a handle used by uniscribe. It is annoying to have the client pass it to every function
  // and manually maintain seprate handle per font.
  SCRIPT_CACHE cache[1];
};
struct LayoutRun {
  int index;
  wchar_t *str_start;
  int char_count;
  SCRIPT_ANALYSIS analysis;
  FontData *font_handle;

  u16 *glyphs;
  int glyph_count;
  u16 *log_clusters;
  SCRIPT_VISATTR *vis_attrs;
  int *advances;
  GOFFSET *g_offsets;
  ABC abc;

  LayoutRun *next;
};
struct LayoutLine {
  LayoutRun *first_run, *last_run;
  int run_count;
  int line_width;
  BYTE *embedding_levels;
  int *visual_to_logical;
  int *logical_to_visual;
  LayoutLine *next;
};
struct LayoutContext {
  HDC dc;
  // temporary buffers for layout and rendering
  ResizableBuffer items[1];
  ResizableBuffer log_attrs[1];
};

struct LayoutParagraph {
  wchar_t *paragraph;
  int paragraph_length;
  LayoutLine *first_line, *last_line;
};

// TODO: RenderAlignment is not implemented yet.
enum RenderAlignment {
  RenderAlignment_Left,
  RenderAlignment_Right,
  RenderAlignment_Center,
};


// This function takes a paragraph (text with no newlines except at the end) and lay it
// out into multiple lines, convert characters to glyphs with shaping and applies the 
// Bidirectional Algorithm for mixed Left-to-Right and Right-to-Left text. This function 
// only need to be called once every time the paragraph or the available width changes.
// This function uses the Windows Uniscribe API.
LayoutParagraph *layout_paragraph(LayoutContext *ctx, FontData *font_data, wchar_t *paragraph, int paragraph_length, int max_line_width);

// This function takes an already layed out paragraph with Uniscribe and renders it.
// It is usually called every frame.
// Currently, this function uses the Uniscribe API for rendering by calling ScriptTextOut
// but it doesn't have to. It can use anything that can draw glyphs by their glyph IDs since
// placement information is provided by the layout process, the glyphs are already shaped
// correctly and provided in the visual order that they supposed to drawn on the line and
// long texts are correctly wrapped into multiple lines.
void render_paragraph(HDC dc, int pos_x, int *cursor_y_ptr, u32 color, LayoutParagraph *p, RenderAlignment text_alignment);
