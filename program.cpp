#include "resizable_buffer.h"
#include "layout.cpp"

struct Paragraph {
  ResizableBuffer text_rb[1];
  wchar_t *text;
  int count;
};


LayoutContext lctx[1] = {};
HBRUSH blue_brush;
HBRUSH black_brush;
HBRUSH outline_brush;

FontData info_font[1];

wchar_t *available_fonts[] = {
  L"Arial",
  L"Courier New",
  L"Consolas",
  L"Tahoma",
  L"Nirmala UI",
  L"Ebrima",
  L"Sylfaen",
};

int current_font_index = 0;
int current_font_size = 56;
FontData current_font[1] = {};

ResizableBuffer paragraphs_rb[1];
Paragraph *paragraphs;
int paragraph_count;


int x_start = 10;
int pos_y = 0;
int max_line_width = 1000;


void update_font(){
  if(current_font->font) {
    DeleteObject(current_font->font);
    current_font->cache[0] = NULL;
  }
  if(current_font->cache) {
    ScriptFreeCache(current_font->cache);
    current_font->cache[0] = NULL;
  }

  current_font->line_height = current_font_size;
  current_font->font_name = available_fonts[current_font_index];
  current_font->font = CreateFontW(
    current_font->line_height, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
    CLEARTYPE_QUALITY, FIXED_PITCH, current_font->font_name
  );
  if(current_font->font == NULL) {
    printf("Error: CreateFontW failed with %x\n", GetLastError());
  }
}
void increase_font_size(){
  current_font_size++;
  if(current_font_size > 200)current_font_size = 200;
  update_font();
}
void decrease_font_size(){
  current_font_size--;
  if(current_font_size < 6)current_font_size = 6;
  update_font();
}
void set_previous_font(){
  current_font_index = (current_font_index + arr_count(available_fonts) - 1) % arr_count(available_fonts);
  update_font();
}
void set_next_font(){
  current_font_index = (current_font_index + 1) % arr_count(available_fonts);
  update_font();
}

void render_paragraph_outline(HDC dc, HBRUSH outline_brush, int pos_x, int cursor_y, LayoutParagraph *p, RenderAlignment text_alignment) {

  for(auto line = p->first_line; line; line = line->next) {
    int cursor_x = pos_x;

    auto max_line_height = 0;
    for(int run_vis_idx=0; run_vis_idx<line->run_count; run_vis_idx++){
      auto run = line->first_run;
      // TODO: IMPORTANT: Get rid of this!!!
      while(run->index != line->visual_to_logical[run_vis_idx])run = run->next;

      if(run->font_handle->line_height > max_line_height)max_line_height = run->font_handle->line_height;

      // B is the absolute width of the ink. A and C are positive for padding and negative for overhang
      // Advance is the absolute width plus padding minus the absolute overhang on both sides.
      int advance = run->abc.abcB + run->abc.abcA + run->abc.abcC;
      RECT rect = {};
      rect.top = cursor_y;
      rect.bottom = cursor_y+run->font_handle->line_height+1;
      rect.left = cursor_x;
      rect.right = cursor_x+advance;
      // printf("%d %d\n", rect.left, rect.right);
      FrameRect(dc, &rect, outline_brush); 
      // break;

      auto g_cursor = cursor_x;
      for(int i=0; i<run->glyph_count; i++) {
        auto g_width = run->advances[i];
        RECT rect = {};
        rect.top = cursor_y+run->font_handle->line_height+2 + 8*(i%4);
        rect.bottom = rect.top + 3; 
        rect.left = g_cursor;
        rect.right = g_cursor+g_width;
        // FrameRect(dc, &rect, brush); 
        g_cursor += g_width;
      }

      cursor_x += advance;
    }
    cursor_y += max_line_height;
  }
}

void layout_and_render_paragraph(LayoutContext *ctx, int *cursor_y_p, wchar_t *paragraph, int paragraph_length){

  if(paragraph_length == 0){
    RECT rect = {};
    rect.top = *cursor_y_p;
    rect.bottom = *cursor_y_p + current_font->line_height+1;
    rect.left = x_start;
    rect.right = x_start+1;
    FrameRect(ctx->dc, &rect, outline_brush); 
    *cursor_y_p += current_font_size;
  } else {
    auto paragraph_layout = layout_paragraph(ctx, current_font, paragraph, paragraph_length, max_line_width);
    render_paragraph_outline(ctx->dc, outline_brush, x_start, *cursor_y_p, paragraph_layout, RenderAlignment_Left);
    render_paragraph(ctx->dc, x_start, cursor_y_p, 0x0000ff, paragraph_layout, RenderAlignment_Left);
    free_layout(paragraph_layout);
  }

  *cursor_y_p += 15;

}

void output_info(LayoutContext *ctx, int x, int y, wchar_t *format, ...){
  va_list args;
  va_start(args, format);
  wchar_t buffer[200];
  int len = vswprintf(buffer, arr_count(buffer)-1, format, args);
  buffer[arr_count(buffer)-1] = 0;
  if(len < 0)len = 0;
  va_end(args);

  auto paragraph_layout = layout_paragraph(lctx, info_font, buffer, len, max_line_width);
  render_paragraph(ctx->dc, x, &y, 0x00ffff, paragraph_layout, RenderAlignment_Left);
  free_layout(paragraph_layout);
}
void render(HWND window, HDC dc) {
  if(dc == NULL){
    printf("Error: HDC is null\n");
  }

  {
    // clear the screen.
    RECT client_rect = {};
    GetClientRect(window, &client_rect);
    FillRect(dc, &client_rect, black_brush);
  }

  {
    // draw the max_line_width marker
    RECT rect = {};
    rect.left = x_start;
    rect.top = -10;
    rect.right = x_start + max_line_width;
    rect.bottom = 1000;
    FrameRect(dc, &rect, blue_brush);
  }

  lctx->dc = dc;

  output_info(lctx, 10, 0*info_font->line_height, L"Font: %s", current_font->font_name);
  output_info(lctx, 10, 1*info_font->line_height, L"Size: %d", current_font->line_height);

  int cursor_y = current_font_size + pos_y;

  for(int i=0; i<paragraph_count; i++) {
    auto p = paragraphs + i;

    layout_and_render_paragraph(lctx, &cursor_y, p->text, p->count);
  }
}

void push_new_paragraph(){
  paragraph_count++;
  paragraphs = rb_ensure_size(paragraphs_rb, paragraph_count, Paragraph);
  auto p = paragraphs + paragraph_count - 1;
  *p = {};
}
Paragraph *get_last_paragraph(){
  if(!paragraph_count) {
    push_new_paragraph();
  }
  return paragraphs + paragraph_count - 1;
}
void free_last_paragraph(){
  auto p = get_last_paragraph();
  if(p) {
    rb_free(p->text_rb);
    paragraph_count--;
  }
}
void add_text_to_paragraph(Paragraph *p, wchar_t *text, int text_length){
  auto new_count = p->count + text_length;
  p->text = rb_ensure_size(p->text_rb, new_count, wchar_t);
  memcpy(p->text+p->count, text, 2*text_length);
  p->count = new_count;
}
void push_paragraph_with_text(wchar_t *text, int text_length = -1){
  if(text_length < 0)text_length = strlen(text);
  push_new_paragraph();
  auto p = get_last_paragraph();
  add_text_to_paragraph(p, text, text_length);
}

void init(HWND window) {
  outline_brush = CreateSolidBrush(0x00ff00);
  blue_brush = CreateSolidBrush(0xff0000);
  black_brush = CreateSolidBrush(0x000000);
  update_font();

  info_font->line_height = 20;
  info_font->font_name = L"Courier New";
  info_font->font = CreateFontW(
    info_font->line_height, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
    CLEARTYPE_QUALITY, FIXED_PITCH, info_font->font_name
  );

  // https://gist.github.com/hqhs/611881e119a55bf3f452b91dc6013c45
  // http://www.manythings.org/bilingual/

  push_paragraph_with_text(L"اَلسَّلَامُ عَلَيْكُمْ");
  push_paragraph_with_text(L"السلام عليكم ورحمة الله وبركاته، كيف حالكم؟");
  push_paragraph_with_text(L"Hello! How are you?");
  push_paragraph_with_text(L"میں ریڈیو کبھی کبھی سنتا ہوں۔");
  push_paragraph_with_text(L"καὶ σὰν πρῶτα ἀνδρειωμένη");
  push_paragraph_with_text(L"מרי אמרה שתכין את שיעורי הבית.");
  push_paragraph_with_text(L"Θέλεις να δοκιμάσεις;");
  // push_paragraph_with_text(L"अपना अच्छे से ख़याल रखना।");
  // push_paragraph_with_text(L"แม่น้ำสายนี้กว้างเท่าไหร่?");
  // push_paragraph_with_text(L"ฝ่ายอ้องอุ้นยุแยกให้แตกกัน");
  // push_paragraph_with_text(L"ሰማይ አይታረስ ንጉሥ አይከሰስ።");
  // push_paragraph_with_text(L"გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო");
  // push_paragraph_with_text(L"اَّلسَّسّسّْلامم َّ");
  // push_paragraph_with_text(L"الله");
  // push_paragraph_with_text(L"السلام عليكم");
  // push_paragraph_with_text(L"السلام عليكم");
  // push_paragraph_with_text(L"Hello World");
  // push_paragraph_with_text(L"car تعني سيارة.");
  // push_paragraph_with_text(L"\u202Bcar تعني سيارة.\u202C");
  // push_paragraph_with_text(L"السلام \t عليكم");
  // push_paragraph_with_text(L"Hello \t wow");
  // push_paragraph_with_text(L"Hello \t\t wow");
  // push_paragraph_with_text(L"Hello \t \t wow");
}
void backspace_command(){
  auto p = get_last_paragraph();
  if(p) {
    if(p->count) {
      p->count--;
    } else {
      free_last_paragraph();
    }
  }
}
void enter_command(){
  push_new_paragraph();
}
void str_command(wchar_t c){
  auto p = get_last_paragraph();
  add_text_to_paragraph(p, &c, 1);
}
void close(HWND window) {
  DeleteObject(outline_brush);
  DeleteObject(blue_brush);
  DeleteObject(black_brush);
  while(paragraph_count)free_last_paragraph();
  rb_free(paragraphs_rb);
  rb_free(lctx->items);
  rb_free(lctx->log_attrs);
}
