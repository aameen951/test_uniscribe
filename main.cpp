#define COBJMACROS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE

#include "my_std.h"
#include <windows.h>
#include <shlwapi.h>
#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <stddef.h>
#include <stdint.h>
#include <intrin.h>
#include <usp10.h>
#include <strsafe.h>
#include <stdarg.h>

#include <intrin.h>

#define ENABLE_SANITY_CHECKS 1

int strlen(const wchar_t *string){
  int len = 0;
  while(*string++)len++;
  return len;
}

struct ResizableBuffer {
  u8 *ptr;
  int size;
};
void *_rb_ensure_size(ResizableBuffer *b, int count, int element_size){
  auto new_size = count * element_size;
  if(new_size > b->size) {
    b->size = new_size;
    b->ptr = (u8 *)realloc(b->ptr, b->size);
  }
  return b->ptr;
}
#define rb_ensure_size(b, count, element_type) ((element_type *)_rb_ensure_size(b, count, sizeof(element_type)))
void rb_free(ResizableBuffer *rb){
  if(rb->ptr){
    free(rb->ptr);
    rb->ptr = NULL;
  }
}

struct LayoutRun {
  int index;
  wchar_t *str_start;
  int char_count;
  SCRIPT_ANALYSIS analysis;

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
};
struct LayoutContext {
  HDC dc;
  HBRUSH outline_brush;

  // temporary buffers for layout and rendering
  ResizableBuffer log_attrs[1];
  ResizableBuffer embedding_levels_rb[1];
  ResizableBuffer map_visual_to_logical_rb[1];
  ResizableBuffer map_logical_to_visual_rb[1];
};



int x_start = 10;
int max_line_width = 300;
int line_height = 50;

void render_paragraph(LayoutContext *ctx, int *cursor_y_ptr, wchar_t *font_name, wchar_t *paragraph) {
  int cursor_y = *cursor_y_ptr;

  auto font = CreateFontW(line_height, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, FIXED_PITCH, font_name);
  // script cache is per font. It is a handle used by uniscribe. It is annoying to have the client pass it to every function
  // and manually maintain seprate handle per font.
  auto font_script_cache = SCRIPT_CACHE{};
  SelectObject(ctx->dc, font);
  
  auto paragraph_length = strlen(paragraph);

  // TODO: Implement text alignment.
  // int text_alignment = 0 /*LEFT*/; // 1 /*RIGHT*/; // 2 /*CENTER*/;
  
  b32 is_rtl_paragraph = false;
  
  auto max_lines = 1000;
  auto lines = (LayoutLine *)calloc(sizeof(LayoutLine), max_lines);
  auto line_count = 0;

  auto max_items = 1000;
  auto _items = (SCRIPT_ITEM * )calloc(sizeof(SCRIPT_ITEM), max_items+1);
  auto script_state = SCRIPT_STATE{};
  auto script_control = SCRIPT_CONTROL{};
  script_state.uBidiLevel = is_rtl_paragraph ? 1 : 0;
  auto _item_count = 0;
  auto result = ScriptItemize(paragraph, paragraph_length, max_items, &script_control, &script_state, _items, &_item_count);
  if(result == 0) {
    // Split items by ranges.

    if(ENABLE_SANITY_CHECKS) {
      for(int i=0; i<_item_count; i++) { 
        if(_items[i].iCharPos >= _items[i+1].iCharPos) {
          printf("Error: item %d is after item %d in text\n", i, i+1);
        }
      }
    }

    LayoutLine *line = NULL;
    auto close_the_line = false;
    // printf("max_line_width: %d\n", max_line_width);

    for(int item_idx=0; item_idx < _item_count; item_idx++) {

      process_the_remaining_of_the_run:

      auto run = (LayoutRun *)calloc(sizeof(LayoutRun), 1);

      auto item = _items + item_idx;
      run->str_start = paragraph + item->iCharPos;
      run->char_count = item[1].iCharPos - item->iCharPos;
      run->analysis = item[0].a;
      auto is_rtl = run->analysis.fRTL;

      auto glyphs_rb = ResizableBuffer{};
      auto log_clusters_rb = ResizableBuffer{};      
      auto vis_attrs_rb = ResizableBuffer{};
      auto advances_rb = ResizableBuffer{};
      auto g_offsets_rb = ResizableBuffer{};

      shape_again:
      auto max_glyphs = 3*run->char_count/2 + 16;

      after_glyph_space_increase:
      run->glyphs = rb_ensure_size(&glyphs_rb, max_glyphs, u16);
      run->log_clusters = rb_ensure_size(&log_clusters_rb, run->char_count, u16);
      run->vis_attrs = rb_ensure_size(&vis_attrs_rb, max_glyphs, SCRIPT_VISATTR);

      after_trying_different_font_or_turning_off_shaping:
      result = ScriptShape(ctx->dc, &font_script_cache, run->str_start, run->char_count, max_glyphs, &run->analysis, run->glyphs, run->log_clusters, run->vis_attrs, &run->glyph_count);
      if(result == 0) {

        if(ENABLE_SANITY_CHECKS) {
          // A single run must not have reordered glyphs.
          for(int i=0; i<run->char_count-1; i++) {
            u16 cluster = run->log_clusters[i];
            u16 next_cluster = run->log_clusters[i+1];
            if(run->analysis.fRTL && cluster < next_cluster) {
              printf("Error: cluster order is smaller than its next in RTL run. (current: %d, next: %d)\n", cluster, next_cluster);
            }
            if(!run->analysis.fRTL && cluster > next_cluster) {
              printf("Error: cluster order is bigger than its next in LTR run. (current: %d, next: %d)\n", cluster, next_cluster);
            }
          }
        }

        run->advances = rb_ensure_size(&advances_rb, run->glyph_count, int);
        run->g_offsets = rb_ensure_size(&g_offsets_rb, run->glyph_count, GOFFSET);
        result = ScriptPlace(ctx->dc, &font_script_cache, run->glyphs, run->glyph_count, run->vis_attrs, &run->analysis, run->advances, run->g_offsets, &run->abc);
        if(result == 0) {
          check_line_again:
          int item_advance = run->abc.abcA + run->abc.abcB + run->abc.abcC;
          auto line_width = line ? line->line_width : 0;
          auto line_width_after = line_width + item_advance;
          // printf("line_width: %d, line_width_after: %d\n", line_width, line_width_after);
          if(line_width_after > max_line_width) {
            close_the_line = true;

            auto found_soft_break = false;
            auto soft_break_char_count = 0;

            // try to find a char in the run to break the line at.
            auto log_attrs = rb_ensure_size(ctx->log_attrs, run->char_count, SCRIPT_LOGATTR);
            result = ScriptBreak(run->str_start, run->char_count, &run->analysis, log_attrs);
            if(result == 0){
              auto current_width = line_width;

              auto g_idx = is_rtl ? run->glyph_count-1 : 0;
              auto end = is_rtl ? -1 : run->glyph_count;
              auto increment = is_rtl ? -1 : 1;
              auto glyph_width_sum = 0;

              // start from 1 because soft breaking on the first character is not useful,
              // we don't want empty runs.
              for(auto char_idx=1; char_idx<run->char_count; char_idx++){

                if(!log_attrs[char_idx].fSoftBreak)continue;

                auto char_glyph_idx = run->log_clusters[char_idx];

                for(; g_idx != end; g_idx += increment) {
                  if(g_idx == char_glyph_idx)break;
                  auto glyph_width = run->advances[g_idx];
                  glyph_width_sum += glyph_width;
                  if(current_width + glyph_width_sum > max_line_width)break;
                }

                if(current_width + glyph_width_sum > max_line_width)break;

                found_soft_break = true;
                soft_break_char_count = char_idx;
                // continue to see if there is further soft breaks that fits on the line.
              }
            } else {
              printf("Error: ScriptBreak failed %x\n", result);
            }
            log_attrs = NULL;

            auto need_to_shape_again = false;
            if(found_soft_break){
              run->char_count = soft_break_char_count;
              // we broke the run into two so we need shape the first part again.
              need_to_shape_again = true;
            } else {
              auto last_glyph_that_fit = is_rtl ? 0xffffff : -1;
              auto current_width = line_width;
              auto start = is_rtl ? run->glyph_count-1 : 0;
              auto end = is_rtl ? -1 : run->glyph_count;
              auto increment = is_rtl ? -1 : 1;
              for(auto i=start; i != end; i += increment) {
                auto glyph_advance = run->advances[i];
                // TODO: Are we allowed to split one cluster into multiple lines? We need two non-zero-width chars in one cluster to test this in notepad.
                if(
                  current_width != 0 && // we must render at least one character on the line even if it doesn't fit.
                  glyph_advance != 0 && // don't make zero-width glyphs go to the next line
                  current_width + glyph_advance > max_line_width
                )break;
                current_width += glyph_advance;
                last_glyph_that_fit = i;
              }

              auto hard_break_char_count = 0;
              for(; hard_break_char_count<run->char_count; hard_break_char_count++) {
                auto glyph = run->log_clusters[hard_break_char_count];
                if(is_rtl ? (glyph < last_glyph_that_fit) : ((int)glyph > last_glyph_that_fit))break;
              }

              if(hard_break_char_count == 0) {
                line = NULL;
                close_the_line = false;
                goto check_line_again;
              } else if(hard_break_char_count < run->char_count) {
                // we broke the run into two so we need shape the first part again.
                need_to_shape_again = true;
                run->char_count = hard_break_char_count;
              }
            }

            if(need_to_shape_again) {
              goto shape_again;
            }
          }

          // allocate a line.
          if(!line) {
            if(line_count+1 < max_lines) {
              line = lines + line_count++;
            }
          }

          if(line) {
            if(ENABLE_SANITY_CHECKS) {
              if(run->char_count == 0) {
                printf("Error: runs cannot have zero chars! (line: %d, run: %d)\n", line_count-1, line->run_count);
              }
            }
            // add the run to the line.
            if(line->first_run)line->last_run = line->last_run->next = run;
            else line->first_run = line->last_run = run;
            run->index = line->run_count++;
            line->line_width += item_advance;
          }

          // if the line is maxed in width close it.
          if(close_the_line) {
            if(ENABLE_SANITY_CHECKS) {
              if(line) {
                if(line->run_count == 0){
                  printf("Error: lines cannot have zero runs! (line: %d)\n", line_count-1);
                }
              }
            }
            line = NULL;
            close_the_line = false;
          }

          // If the run was splitted then we need to process the remaining of run as separate run.
          if(item[0].iCharPos+run->char_count != item[1].iCharPos) {
            item->iCharPos += run->char_count;
            goto process_the_remaining_of_the_run;
          }

        } else {
          // According to the documentation this should not happen unless the font was broken.
          printf("Error: ScriptPlace failed %x\n", result);
        }
      } else {
        if(result == E_OUTOFMEMORY) {
          max_glyphs *= 2;
          printf("max_glyphs increased to %d\n", max_glyphs);
          goto after_glyph_space_increase;
        } else if(result == USP_E_SCRIPT_NOT_IN_FONT) {
          // NOTE: Shaping requires information from the font to map characters to the right glyph form (The GSUB and GPOS tables in OpenType font).
          // For now we will just turn off shaping for the run if the font doesn't support the script.
          // TODO: The documentation is suggesting to try different font with shaping until it succeeds which sounds very bad and slow. 
          // maybe we can use ScriptGetFontScriptTags to check if a font support the script 
          // TODO: Handle fallback font.
          run->analysis.eScript = SCRIPT_UNDEFINED;
          goto after_trying_different_font_or_turning_off_shaping;
        } 
        printf("Error: ScriptShape failed %x\n", result);
      }
    }

    /// Drawing
    SetBkMode(ctx->dc, TRANSPARENT);
    SetTextColor(ctx->dc, 0x0000ff);

    for(int line_idx=0; line_idx<line_count; line_idx++) {
      auto line = lines + line_idx;

      // can we store this data?
      auto embedding_levels = rb_ensure_size(ctx->embedding_levels_rb, line->run_count, BYTE);
      for(auto run=line->first_run; run; run=run->next) {
        embedding_levels[run->index] = run->analysis.s.uBidiLevel;
      }

      auto map_visual_to_logical = rb_ensure_size(ctx->map_visual_to_logical_rb, line->run_count, int);
      auto map_logical_to_visual = rb_ensure_size(ctx->map_logical_to_visual_rb, line->run_count, int);
      result = ScriptLayout(line->run_count, embedding_levels, map_visual_to_logical, map_logical_to_visual);
      if(result != 0) {
        printf("Error: ScriptLayout failed %x\n", result);
      }

      for(int i=0; i<line->run_count; i++) {
        if(i != map_visual_to_logical[map_logical_to_visual[i]]) {
          printf("Error: ScriptLayout is not returning expected result for i:%d\n", i);
        }
        if(i != map_logical_to_visual[map_visual_to_logical[i]]) {
          printf("Error: ScriptLayout is not returning expected result for i:%d\n", i);
        }
      }
      
      int cursor_x = x_start;
      for(int run_vis_idx=0; run_vis_idx<line->run_count; run_vis_idx++){
        auto run = line->first_run;
        // TODO: IMPORTANT: Get rid of this!!!
        while(run->index != map_visual_to_logical[run_vis_idx])run = run->next;

        // B is the absolute width of the ink. A and C are positive for padding and negative for overhang
        // Advance is the absolute width plus padding minus the absolute overhang on both sides.
        int advance = run->abc.abcB + run->abc.abcA + run->abc.abcC;
        RECT rect = {};
        rect.top = cursor_y;
        rect.bottom = cursor_y+line_height+1;
        rect.left = cursor_x;
        rect.right = cursor_x+advance;
        // printf("%d %d\n", rect.left, rect.right);
        FrameRect(ctx->dc, &rect, ctx->outline_brush); 
        // break;

        auto g_cursor = cursor_x;
        for(int i=0; i<run->glyph_count; i++) {
          auto g_width = run->advances[i];
          RECT rect = {};
          rect.top = cursor_y+line_height+2 + 8*(i%4);
          rect.bottom = rect.top + 3; 
          rect.left = g_cursor;
          rect.right = g_cursor+g_width;
          // FrameRect(dc, &rect, brush); 
          g_cursor += g_width;
        }

        cursor_x += advance;
      }

      cursor_x = x_start;
      for(int run_vis_idx=0; run_vis_idx<line->run_count; run_vis_idx++){
        auto run = line->first_run;
        // TODO: IMPORTANT: Get rid of this!!!
        while(run->index != map_visual_to_logical[run_vis_idx])run = run->next;

        // B is the absolute width of the ink. A and C are positive for padding and negative for overhang
        // Advance is the absolute width plus padding minus the absolute overhang on both sides.
        int advance = run->abc.abcB + run->abc.abcA + run->abc.abcC;
        result = ScriptTextOut(ctx->dc, &font_script_cache, cursor_x, cursor_y, 0, NULL, &run->analysis, NULL, 0, run->glyphs, run->glyph_count, run->advances, NULL, run->g_offsets);
        if(result != 0) {
          printf("Error: ScriptTextOut failed %x\n", result);
        }
        cursor_x += advance;
      }
      cursor_y += line_height;

    }

  } else {
    printf("Error: ScriptItemize failed %x\n", result);
  }
  *cursor_y_ptr = cursor_y;

  // free everything
  for(int line_idx=0; line_idx < line_count; line_idx++){
    auto line = lines + line_idx;
    auto next_run = line->first_run;
    for(auto run=next_run; run; run = next_run) {
      free(run->advances);
      free(run->glyphs);
      free(run->g_offsets);
      free(run->log_clusters);
      free(run->vis_attrs);
      next_run = run->next;
      free(run);
    }
  }
  free(lines);
  free(_items);
  ScriptFreeCache(&font_script_cache);
  DeleteObject(font);
}

b32 done = false;
LayoutContext lctx[1] = {};

void render(HWND window, HDC dc) {
  if(done)return;

  auto blue_brush = CreateSolidBrush(0xff0000);
  {
    // draw the max_line_width marker
    RECT rect = {};
    rect.left = x_start;
    rect.top = -10;
    rect.right = x_start + max_line_width;
    rect.bottom = 1000;
    FrameRect(dc, &rect, blue_brush);
  }

  lctx->outline_brush = CreateSolidBrush(0x00ff00);
  lctx->dc = dc;

  int cursor_y = line_height;
  
  // https://gist.github.com/hqhs/611881e119a55bf3f452b91dc6013c45
  // http://www.manythings.org/bilingual/

  // render_paragraph(lctx, &cursor_y, L"Courier New", L"السلام عليكم ورحمة الله وبركاته، كيف حالكم؟");
  // render_paragraph(lctx, &cursor_y, L"Arial",       L"اَلسَّلاْم");
  // render_paragraph(lctx, &cursor_y, L"Arial",       L"اَّلسَّسّسّْلامم َّ");
  // render_paragraph(lctx, &cursor_y, L"Nirmala UI",  L"अपना अच्छे से ख़याल रखना।");
  // render_paragraph(lctx, &cursor_y, L"Arial",       L"میں ریڈیو کبھی کبھی سنتا ہوں۔");
  // render_paragraph(lctx, &cursor_y, L"Tahoma",      L"แม่น้ำสายนี้กว้างเท่าไหร่?");
  // render_paragraph(lctx, &cursor_y, L"Tahoma",      L"ฝ่ายอ้องอุ้นยุแยกให้แตกกัน");
  // render_paragraph(lctx, &cursor_y, L"Ebrima",      L"ሰማይ አይታረስ ንጉሥ አይከሰስ።");
  // render_paragraph(lctx, &cursor_y, L"Sylfaen",     L"გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო");
  // render_paragraph(lctx, &cursor_y, L"Arial",       L"καὶ σὰν πρῶτα ἀνδρειωμένη");
  // render_paragraph(lctx, &cursor_y, L"Arial",       L"מרי אמרה שתכין את שיעורי הבית.");
  // render_paragraph(lctx, &cursor_y, L"Arial",       L"Θέλεις να δοκιμάσεις;");
  // render_paragraph(lctx, &cursor_y, L"Arial",       L"الله");
  // render_paragraph(lctx, &cursor_y, L"Arial",       L"اَلَسَلََّامَّ عَّلَّيكم xxx xxx xxx xxx");
  render_paragraph(lctx, &cursor_y, L"Arial",       L"السلام عليكم");

  done = true;
  DeleteObject(blue_brush);
}
static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
  LRESULT Result = 0;

  switch (message)
  {
    case WM_CLOSE: {
      DestroyWindow(window);
    }break;
    case WM_DESTROY:
    {
      PostQuitMessage(0);
    } break;

    case WM_KEYDOWN: {
      switch(w_param){
        case VK_RIGHT: {
          max_line_width += GetKeyState(VK_SHIFT) & 0x8000 ? 10 : 1;
        }break;
        case VK_LEFT: {
          max_line_width -= GetKeyState(VK_SHIFT) & 0x8000 ? 10 : 1;
        }break;
      }
      InvalidateRect(window, NULL, TRUE);
      UpdateWindow(window);
    }break;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(window, &ps);
      done = false;
      render(window, dc);
      EndPaint(window, &ps);
    } break;

    default:
    {
      Result = DefWindowProcW(window, message, w_param, l_param);
    } break;
  }

  return Result;
}

typedef BOOL WINAPI set_process_dpi_aware(void);
typedef BOOL WINAPI set_process_dpi_awareness_context(DPI_AWARENESS_CONTEXT);
static void PreventWindowsDPIScaling()
{
  HMODULE WinUser = LoadLibraryW(L"user32.dll");
  set_process_dpi_awareness_context *SetProcessDPIAwarenessContext = (set_process_dpi_awareness_context *)GetProcAddress(WinUser, "SetProcessDPIAwarenessContext");
  if(SetProcessDPIAwarenessContext)
  {
    SetProcessDPIAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
  }
  else
  {
    set_process_dpi_aware *SetProcessDPIAware = (set_process_dpi_aware *)GetProcAddress(WinUser, "SetProcessDPIAware");
    if(SetProcessDPIAware)
    {
      SetProcessDPIAware();
    }
  }
}

static HWND CreateOutputWindow()
{
  WNDCLASSEXW WindowClass = {};
  WindowClass.cbSize = sizeof(WindowClass);
  WindowClass.lpfnWndProc = &window_proc;
  WindowClass.hInstance = GetModuleHandleW(NULL);
  WindowClass.hIcon = LoadIconA(NULL, IDI_APPLICATION);
  WindowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);
  WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  WindowClass.lpszClassName = L"reftermclass";

  HWND Result = {0};
  if(RegisterClassExW(&WindowClass))
  {
    DWORD ExStyle = WS_EX_APPWINDOW;
    Result = CreateWindowExW(ExStyle, WindowClass.lpszClassName, L"refterm", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                              0, 0, WindowClass.hInstance, 0);
  }

  return Result;
}

int main() {
  // PreventWindowsDPIScaling();

  HWND Window = CreateOutputWindow();
  ShowWindow(Window, SW_SHOW);

  MSG Message;
  while(GetMessageW(&Message, 0, 0, 0)) {
    TranslateMessage(&Message);
    DispatchMessageW(&Message);
  }
}
