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
  // for(int i=0; i<strlen(paragraph); i++) {
  //   printf("%x ", paragraph[i]); 
  // }
  // printf("\n"); 
  auto paragraph_layout = layout_paragraph(ctx, &data, paragraph, max_line_width);
  // for(auto run=paragraph_layout->first_line->first_run; run; run = run->next){
  //   for(auto i=0; i<run->glyph_count; i++){
  //     printf("%x ", run->glyphs[i]); 
  //   }
  // printf("| "); 
  // }
  // printf("\n"); 
  // printf("\n"); 
  render_paragraph_outline(ctx->dc, outline_brush, x_start, *cursor_y_p, paragraph_layout, RenderAlignment_Left);
  render_paragraph(ctx->dc, x_start, cursor_y_p, paragraph_layout, RenderAlignment_Left);
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

  // layout_and_render_paragraph(lctx, &cursor_y, L"Courier New", L"XXX XXX XXX");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Courier New", L"السلام عليكم ورحمة الله وبركاته، كيف حالكم؟");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"اَلسَّلاْم");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"اَّلسَّسّسّْلامم َّ");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Nirmala UI",  L"अपना अच्छे से ख़याल रखना।");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"میں ریڈیو کبھی کبھی سنتا ہوں۔");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Tahoma",      L"แม่น้ำสายนี้กว้างเท่าไหร่?");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Tahoma",      L"ฝ่ายอ้องอุ้นยุแยกให้แตกกัน");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Ebrima",      L"ሰማይ አይታረስ ንጉሥ አይከሰስ።");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Sylfaen",     L"გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"καὶ σὰν πρῶτα ἀνδρειωμένη");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"מרי אמרה שתכין את שיעורי הבית.");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"Θέλεις να δοκιμάσεις;");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"الله");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"اَلَسَلََّامَّ عَّلَّيكم xxx xxx xxx xxx");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"السلام عليكم");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"Hello World");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"car تعني سيارة.");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"\u202Bcar تعني سيارة.\u202C");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"السلام \t عليكم");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"Hello \t wow");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"Hello \t\t wow");
  // layout_and_render_paragraph(lctx, &cursor_y, L"Arial",       L"Hello \t \t wow");

}
