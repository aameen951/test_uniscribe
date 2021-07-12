#include "resizable_buffer.h"
#include "layout.cpp"

LayoutContext lctx[1] = {};
HBRUSH blue_brush;
HBRUSH black_brush;
HBRUSH outline_brush;

void init(HWND window) {
  outline_brush = CreateSolidBrush(0x00ff00);
  blue_brush = CreateSolidBrush(0xff0000);
  black_brush = CreateSolidBrush(0x000000);
}
void close(HWND window) {
  DeleteObject(outline_brush);
  DeleteObject(blue_brush);
  DeleteObject(black_brush);
  rb_free(lctx->items);
  rb_free(lctx->log_attrs);
}
int x_start = 10;
int pos_y = 0;
int max_line_width = 300;
int line_height = 50;

void layout_and_render_paragraph(LayoutContext *ctx, int *cursor_y_p, wchar_t *font_name, wchar_t *paragraph){

  FontData data = {};
  data.line_height = line_height;
  data.font_name = font_name;
  data.font = CreateFontW(line_height, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, FIXED_PITCH, font_name);
  if(data.font == NULL) {
    printf("Error: CreateFontW failed with %x\n", GetLastError());
  }
  // script cache is per font. It is a handle used by uniscribe. It is annoying to have the client pass it to every function
  // and manually maintain seprate handle per font.
  *data.cache = {};

  auto paragraph_layout = layout_paragraph(ctx, &data, paragraph, max_line_width);
  render_paragraph(ctx->dc, outline_brush, x_start, cursor_y_p, paragraph_layout, RenderAlignment_Left);
  free_layout(paragraph_layout);

  *cursor_y_p += 15;

  ScriptFreeCache(data.cache);
  DeleteObject(data.font);
}
void render(HWND window, HDC dc) {
  if(dc == NULL){
    printf("Error: HDC is null\n");
  }

  {
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

  int cursor_y = line_height + pos_y;
  
  // https://gist.github.com/hqhs/611881e119a55bf3f452b91dc6013c45
  // http://www.manythings.org/bilingual/

  layout_and_render_paragraph(lctx, &cursor_y, L"Courier New", L"السلام عليكم ورحمة الله وبركاته، كيف حالكم؟");
  layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"اَلسَّلاْم");
  layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"اَّلسَّسّسّْلامم َّ");
  layout_and_render_paragraph(lctx, &cursor_y, L"Nirmala UI",  L"अपना अच्छे से ख़याल रखना।");
  layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"میں ریڈیو کبھی کبھی سنتا ہوں۔");
  layout_and_render_paragraph(lctx, &cursor_y, L"Tahoma",      L"แม่น้ำสายนี้กว้างเท่าไหร่?");
  layout_and_render_paragraph(lctx, &cursor_y, L"Tahoma",      L"ฝ่ายอ้องอุ้นยุแยกให้แตกกัน");
  layout_and_render_paragraph(lctx, &cursor_y, L"Ebrima",      L"ሰማይ አይታረስ ንጉሥ አይከሰስ።");
  layout_and_render_paragraph(lctx, &cursor_y, L"Sylfaen",     L"გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო");
  layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"καὶ σὰν πρῶτα ἀνδρειωμένη");
  layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"מרי אמרה שתכין את שיעורי הבית.");
  layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"Θέλεις να δοκιμάσεις;");
  layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"الله");
  layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"اَلَسَلََّامَّ عَّلَّيكم xxx xxx xxx xxx");
  layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"السلام عليكم");

}
