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


int x_start = 10;
int max_line_width = 300;
int line_height = 50;

void render_paragraph(HDC dc, int *cursor_y_ptr, wchar_t *font_name, wchar_t *paragraph) {
  int cursor_y = *cursor_y_ptr;
  auto brush = CreateSolidBrush(0x00ff00);


  auto font = CreateFontW(line_height, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, FIXED_PITCH, font_name);
  SelectObject(dc, font);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, 0x0000ff);
  
  auto paragraph_length = strlen(paragraph);
  // TextOutW(dc, x_start, 0, paragraph, paragraph_length);

  b32 is_rtl = false;
  
  auto max_lines = 1000;
  auto lines = (LayoutLine *)calloc(sizeof(LayoutLine), max_lines);
  auto line_count = 0;

  auto script_cache = SCRIPT_CACHE{};
  auto max_items = 1000;
  auto _items = (SCRIPT_ITEM * )calloc(sizeof(SCRIPT_ITEM), max_items+1);
  auto script_state = SCRIPT_STATE{};
  auto script_control = SCRIPT_CONTROL{};
  script_state.uBidiLevel = is_rtl ? 1 : 0;
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

      shape_again:

      auto max_glyphs = 3*run->char_count/2 + 16;
      run->glyphs = (u16 *)calloc(2, max_glyphs);
      run->log_clusters = (u16 *)calloc(2, run->char_count);
      run->vis_attrs = (SCRIPT_VISATTR *)calloc(sizeof(SCRIPT_VISATTR), max_glyphs);
      result = ScriptShape(dc, &script_cache, run->str_start, run->char_count, max_glyphs, &run->analysis, run->glyphs, run->log_clusters, run->vis_attrs, &run->glyph_count);
      if(result == 0) {

        if(ENABLE_SANITY_CHECKS) {
          // A single run must not have reordered glyphs.
          for(int i=0; i<run->char_count-1; i++) {
            u16 cluster = run->log_clusters[i];
            u16 next_cluster = run->log_clusters[i+1];
            if(run->analysis.fRTL && cluster < next_cluster) {
              printf("Error: cluster is smaller than its next in RTL run. (current: %d, next: %d)\n", cluster, next_cluster);
            }
            if(!run->analysis.fRTL && cluster > next_cluster) {
              printf("Error: cluster is bigger than its next in LTR run. (current: %d, next: %d)\n", cluster, next_cluster);
            }
          }
        }

        run->advances = (int *)calloc(sizeof(int), run->glyph_count);
        run->g_offsets = (GOFFSET *)calloc(sizeof(GOFFSET), run->glyph_count);
        result = ScriptPlace(dc, &script_cache, run->glyphs, run->glyph_count, run->vis_attrs, &run->analysis, run->advances, run->g_offsets, &run->abc);
        if(result == 0) {
          check_line_again:
          int item_advance = run->abc.abcA + run->abc.abcB + run->abc.abcC;
          auto line_width = line ? line->line_width : 0;
          auto line_width_after = line_width + item_advance;
          // printf("line_width: %d, line_width_after: %d\n", line_width, line_width_after);
          if(line_width_after > max_line_width) {
            close_the_line = true;
            auto log_attrs = (SCRIPT_LOGATTR *)calloc(sizeof(SCRIPT_LOGATTR), run->char_count);
            // TODO: use this!
            result = ScriptBreak(run->str_start, run->char_count, &run->analysis, log_attrs);
            if(result == 0){

            }
            if(result == 0){
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

              auto char_count_that_fit = 0;
              for(; char_count_that_fit<run->char_count; char_count_that_fit++) {
                auto glyph = run->log_clusters[char_count_that_fit];
                if(is_rtl ? (glyph < last_glyph_that_fit) : ((int)glyph > last_glyph_that_fit))break;
              }

              if(char_count_that_fit == 0) {
                line = NULL;
                close_the_line = false;
                goto check_line_again;
              } else if(char_count_that_fit < run->char_count) {
                run->char_count = char_count_that_fit;
                free(run->glyphs);
                free(run->log_clusters);
                free(run->vis_attrs);
                free(run->advances);
                free(run->g_offsets);
                free(log_attrs);
                goto shape_again;
              }

            } else {
              printf("Error: ScriptBreak failed %x\n", result);
            }
            free(log_attrs);
          }

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
            if(line->first_run)line->last_run = line->last_run->next = run;
            else line->first_run = line->last_run = run;
            run->index = line->run_count++;
            line->line_width += item_advance;
          }

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

          if(item[0].iCharPos+run->char_count != item[1].iCharPos) {
            item->iCharPos += run->char_count;
            goto process_the_remaining_of_the_run;
          }

        } else {
          printf("Error: ScriptPlace failed %x\n", result);
        }
      } else {
        printf("Error: ScriptShape failed %x\n", result);
      }
    }
    for(int i=0; i<line_count; i++){
      // printf("Line %d: %d\n", i, lines[i].run_count);
    }

    /// Drawing

    for(int line_idx=0; line_idx<line_count; line_idx++) {
      auto line = lines + line_idx;

      // can we store this data?
      auto embedding_levels = (BYTE *)calloc(sizeof(BYTE), line->run_count);
      for(auto run=line->first_run; run; run=run->next) {
        embedding_levels[run->index] = run->analysis.s.uBidiLevel;
      }

      auto map_visual_to_logical = (int *)calloc(sizeof(int), line->run_count);
      auto map_logical_to_visual = (int *)calloc(sizeof(int), line->run_count);
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
        FrameRect(dc, &rect, brush); 
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
        result = ScriptTextOut(dc, &script_cache, cursor_x, cursor_y, 0, NULL, &run->analysis, NULL, 0, run->glyphs, run->glyph_count, run->advances, NULL, run->g_offsets);
        if(result != 0) {
          printf("Error: ScriptTextOut failed %x\n", result);
        }
        cursor_x += advance;
      }
      cursor_y += line_height;

      free(embedding_levels);
      free(map_visual_to_logical);
      free(map_logical_to_visual);
    }

  } else {
    printf("Error: ScriptItemize failed %x\n", result);
  }
  *cursor_y_ptr = cursor_y;

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
  DeleteObject(font);
  DeleteObject(brush);
}
b32 done = false;
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

  int cursor_y = line_height;
  
  // https://gist.github.com/hqhs/611881e119a55bf3f452b91dc6013c45

  // render_paragraph(dc, &cursor_y, L"Courier New", L"السلام عليكم ورحمة الله وبركاته، كيف حالكم؟");
  // render_paragraph(dc, &cursor_y, L"Arial",       L"اَلسَّلاْم");
  // render_paragraph(dc, &cursor_y, L"Arial",       L"اَّلسَّسّسّْلامم َّ");
  // render_paragraph(dc, &cursor_y, L"Nirmala UI",  L"अपना अच्छे से ख़याल रखना।");
  // render_paragraph(dc, &cursor_y, L"Arial",       L"میں ریڈیو کبھی کبھی سنتا ہوں۔");
  // render_paragraph(dc, &cursor_y, L"Tahoma",      L"แม่น้ำสายนี้กว้างเท่าไหร่?");
  // render_paragraph(dc, &cursor_y, L"Tahoma",      L"ฝ่ายอ้องอุ้นยุแยกให้แตกกัน");
  // render_paragraph(dc, &cursor_y, L"Ebrima",      L"ሰማይ አይታረስ ንጉሥ አይከሰስ።");
  // render_paragraph(dc, &cursor_y, L"Sylfaen",     L"გთხოვთ ახლავე გაიაროთ რეგისტრაცია Unicode-ის მეათე საერთაშორისო");
  // render_paragraph(dc, &cursor_y, L"Arial",       L"καὶ σὰν πρῶτα ἀνδρειωμένη");
  // render_paragraph(dc, &cursor_y, L"Arial",       L"מרי אמרה שתכין את שיעורי הבית.");
  // render_paragraph(dc, &cursor_y, L"Arial",       L"Θέλεις να δοκιμάσεις;");
  render_paragraph(dc, &cursor_y, L"Arial",       L"الله");

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
