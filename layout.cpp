#include "my_std.h"
#include "resizable_buffer.h"

#ifndef ENABLE_SANITY_CHECKS
#define ENABLE_SANITY_CHECKS 0
#endif

struct FontData {
  int line_height;
  wchar_t *font_name;
  HFONT font;
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

enum RenderAlignment {
  RenderAlignment_Left,
  RenderAlignment_Right,
  RenderAlignment_Center,
};


struct FindLineBreakResult {
  b32 found;
  int char_count;
  int width;
};
FindLineBreakResult find_soft_break(LayoutContext *ctx, LayoutRun *run, int available_width) {
  FindLineBreakResult result = {};

  // try to find a char in the run to break the line at.
  auto log_attrs = rb_ensure_size(ctx->log_attrs, run->char_count, SCRIPT_LOGATTR);
  auto res = ScriptBreak(run->str_start, run->char_count, &run->analysis, log_attrs);
  if(res != S_OK){
    printf("Error: ScriptBreak failed %x\n", res);
    return result;
  }

  auto is_rtl = run->analysis.fRTL;
  auto g_idx = is_rtl ? run->glyph_count-1 : 0;
  auto end = is_rtl ? -1 : run->glyph_count;
  auto increment = is_rtl ? -1 : 1;
  auto current_width = 0;

  // start from 1 because soft breaking on the first character is not useful,
  // we don't want empty runs.
  for(auto char_idx=1; char_idx<run->char_count; char_idx++){

    if(!log_attrs[char_idx].fSoftBreak)continue;

    auto char_glyph_idx = run->log_clusters[char_idx];

    for(; g_idx != end; g_idx += increment) {
      if(g_idx == char_glyph_idx)break;
      auto glyph_width = run->advances[g_idx];
      current_width += glyph_width;
      if(current_width > available_width)break;
    }

    if(current_width > available_width)break;

    result.found = true;
    result.char_count = char_idx;
    result.width = current_width;
    // continue to see if there is another soft breaks that fits on the line.
  }

  if(result.found) {
    // whitespaces are allowed to go over the maximum line width.
    while(result.char_count < run->char_count && log_attrs[result.char_count].fWhiteSpace){
      result.char_count++;
    }
  }

  return result;
}
FindLineBreakResult find_hard_break(LayoutRun *run, int available_width){
  FindLineBreakResult result = {};

  // TODO: Are we allowed to split one cluster into multiple lines?

  auto is_rtl = run->analysis.fRTL;

  auto start = is_rtl ? run->glyph_count-1 : 0;
  auto end = is_rtl ? -1 : run->glyph_count;
  auto increment = is_rtl ? -1 : 1;

  auto glyph_idx = start;

  auto current_width = 0;
  for(auto char_idx=0; char_idx<run->char_count; char_idx++){
    auto char_glyph_idx = run->log_clusters[char_idx];

    int char_width = 0;
    for(; glyph_idx != end; glyph_idx += increment) {
      auto glyph_advance = run->advances[glyph_idx];
      char_width += glyph_advance;
      if(glyph_idx == char_glyph_idx)break;
    }

    if(
      char_width + current_width > available_width &&
      char_width != 0 &&  // always include zero-width glyphs in the same line
      current_width != 0  // we must render at least one character on the line even if it doesn't fit.
    )break;

    current_width += char_width;

    result.found = true;
    result.char_count = char_idx + 1;
    result.width = current_width;
  }

  return result;
}

LayoutParagraph *layout_paragraph(LayoutContext *ctx, FontData *font_data, wchar_t *paragraph, int max_line_width) {
  auto result = (LayoutParagraph *)calloc(1, sizeof(LayoutParagraph));

  SelectObject(ctx->dc, font_data->font);
  
  auto paragraph_length = strlen(paragraph);
  
  b32 is_rtl_paragraph = false;
  
  auto script_state = SCRIPT_STATE{};
  auto script_control = SCRIPT_CONTROL{};
  script_state.uBidiLevel = is_rtl_paragraph ? 1 : 0;

  auto max_items = 16;
  auto item_count = 0;
  itemize_again:
  auto items = rb_ensure_size(ctx->items, max_items, SCRIPT_ITEM);
  auto res = ScriptItemize(paragraph, paragraph_length, max_items, &script_control, &script_state, items, &item_count);
  if(res == E_OUTOFMEMORY){
    max_items *= 2;
    goto itemize_again;
  } else if(res != S_OK) {
    printf("Error: ScriptItemize failed %x\n", res);
    return result;
  }

  if(ENABLE_SANITY_CHECKS) {
    // make sure characters in items are consecutive.
    for(int i=0; i<item_count; i++) { 
      if(items[i].iCharPos >= items[i+1].iCharPos) {
        printf("Error: item %d is after item %d in text\n", i, i+1);
      }
    }
  }

  LayoutLine *line = NULL;
  auto close_the_line = false;

  for(int item_idx=0; item_idx < item_count; item_idx++) {

    process_the_remaining_of_the_run:

    auto run = (LayoutRun *)calloc(sizeof(LayoutRun), 1);

    auto item = items + item_idx;
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
    run->vis_attrs = rb_ensure_size(&vis_attrs_rb, max_glyphs, SCRIPT_VISATTR);
    run->log_clusters = rb_ensure_size(&log_clusters_rb, run->char_count, u16);

    after_trying_different_font_or_turning_off_shaping:
    res = ScriptShape(ctx->dc, font_data->cache, run->str_start, run->char_count, max_glyphs, &run->analysis, run->glyphs, run->log_clusters, run->vis_attrs, &run->glyph_count);
    if(res == E_OUTOFMEMORY) {
      max_glyphs *= 2;
      printf("max_glyphs increased to %d\n", max_glyphs);
      goto after_glyph_space_increase;
    } else if(res == USP_E_SCRIPT_NOT_IN_FONT) {
      // NOTE: Shaping requires information from the font to map characters to the right glyph form (The GSUB and GPOS tables in OpenType font).
      // For now we will just turn off shaping for the run if the font doesn't support the script.
      // TODO: The documentation is suggesting to try different font with shaping until it succeeds which sounds very bad and slow. 
      // maybe we can use ScriptGetFontScriptTags to check if a font support the script 
      // TODO: Handle fallback font.
      printf("Error: disabling shaping for run\n", res);
      run->analysis.eScript = SCRIPT_UNDEFINED;
      goto after_trying_different_font_or_turning_off_shaping;
    } else if(res != S_OK) {
      printf("Error: ScriptShape failed %x\n", res);
      rb_free(&glyphs_rb);
      rb_free(&log_clusters_rb);
      rb_free(&vis_attrs_rb);
      free(run);
      continue;
    }


    run->font_handle = font_data;

    #ifdef ENABLE_SANITY_CHECKS
    {
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
    #endif

    run->advances = rb_ensure_size(&advances_rb, run->glyph_count, int);
    run->g_offsets = rb_ensure_size(&g_offsets_rb, run->glyph_count, GOFFSET);
    res = ScriptPlace(ctx->dc, font_data->cache, run->glyphs, run->glyph_count, run->vis_attrs, &run->analysis, run->advances, run->g_offsets, &run->abc);
    if(res != S_OK) {
      // According to the documentation this should not happen unless the font was broken.
      printf("Error: ScriptPlace failed %x\n", res);
      rb_free(&glyphs_rb);
      rb_free(&log_clusters_rb);
      rb_free(&vis_attrs_rb);
      rb_free(&advances_rb);
      rb_free(&g_offsets_rb);
      free(run);
      continue;
    }

    check_line_again:
    int item_advance = run->abc.abcA + run->abc.abcB + run->abc.abcC;
    auto line_width = line ? line->line_width : 0;
    auto line_width_after = line_width + item_advance;

    // printf("line_width: %d, line_width_after: %d\n", line_width, line_width_after);
    if(line_width_after > max_line_width) {
      close_the_line = true;

      auto soft_break_res = find_soft_break(ctx, run, max_line_width - line_width);
      if(soft_break_res.found){
        run->char_count = soft_break_res.char_count;
        // we broke the run into two so we need shape the first part again.
        goto shape_again;
      } else if(line_width == 0) {

        // if there is nothing on the line then hard break a run to prevent infinitely looping.
        auto hard_break_res = find_hard_break(run, max_line_width - line_width);

        if(hard_break_res.char_count < run->char_count) {
          // we broke the run into two so we need shape the first part again.
          run->char_count = hard_break_res.char_count;
          goto shape_again;
        }

      } else {
        // The line is not empty and the entire run doesn't fit then
        // close the line and try to fit this run on the new line.
        line = NULL;
        close_the_line = false;
        goto check_line_again;
      }
    }

    // we need a line, allocate one if we don't have one.
    if(!line) {
      line = (LayoutLine *)calloc(1, sizeof(LayoutLine));
      if(result->first_line)result->last_line = result->last_line->next = line;
      else result->first_line = result->last_line = line;
    }

    if(ENABLE_SANITY_CHECKS && run->char_count == 0) {
      printf("Error: runs cannot have zero chars! (run: %d)\n", line->run_count);
    }

    if(line) {
      // add the run to the line.
      if(line->first_run)line->last_run = line->last_run->next = run;
      else line->first_run = line->last_run = run;
      run->index = line->run_count++;
      line->line_width += item_advance;
    }

    // if the line is maxed in width close it.
    if(close_the_line) {
      if(ENABLE_SANITY_CHECKS && line && line->run_count == 0) {
        printf("Error: lines cannot have zero runs!\n");
      }
      line = NULL;
      close_the_line = false;
    }

    // If the run was splitted then we need to process the remaining of run as separate run.
    if(item[0].iCharPos+run->char_count != item[1].iCharPos) {
      item->iCharPos += run->char_count;
      goto process_the_remaining_of_the_run;
    }
  }

  for(auto line=result->first_line; line; line = line->next) {
    line->embedding_levels = (BYTE *)calloc(line->run_count, sizeof(BYTE));
    for(auto run=line->first_run; run; run=run->next) {
      line->embedding_levels[run->index] = run->analysis.s.uBidiLevel;
    }

    line->visual_to_logical = (int *)calloc(line->run_count, sizeof(int));
    line->logical_to_visual = (int *)calloc(line->run_count, sizeof(int));
    auto result = ScriptLayout(line->run_count, line->embedding_levels, line->visual_to_logical, line->logical_to_visual);
    if(result != 0) {
      printf("Error: ScriptLayout failed with %x\n", result);
      continue;
    }

    if(ENABLE_SANITY_CHECKS) {
      for(int i=0; i<line->run_count; i++) {
        if(i != line->visual_to_logical[line->logical_to_visual[i]]) {
          printf("Error: ScriptLayout is not returning expected result for i:%d\n", i);
        }
        if(i != line->logical_to_visual[line->visual_to_logical[i]]) {
          printf("Error: ScriptLayout is not returning expected result for i:%d\n", i);
        }
      }
    }
  }

  return result;
}

void render_paragraph(HDC dc, HBRUSH outline_brush, int pos_x, int *cursor_y_ptr, LayoutParagraph *p, RenderAlignment text_alignment) {
  int cursor_y = *cursor_y_ptr;
  
  // TODO: Implement text alignment.
  
  /// Drawing
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, 0x0000ff);

  for(auto line = p->first_line; line; line = line->next) {

    int cursor_x = pos_x;
    for(int run_vis_idx=0; run_vis_idx<line->run_count; run_vis_idx++){
      auto run = line->first_run;
      // TODO: IMPORTANT: Get rid of this!!!
      while(run->index != line->visual_to_logical[run_vis_idx])run = run->next;

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

    cursor_x = pos_x;
    auto max_line_height = 0;
    for(int run_vis_idx=0; run_vis_idx<line->run_count; run_vis_idx++){
      auto run = line->first_run;
      // TODO: IMPORTANT: Get rid of this!!!
      while(run->index != line->visual_to_logical[run_vis_idx])run = run->next;

      if(run->font_handle->line_height > max_line_height)max_line_height = run->font_handle->line_height;

      SelectObject(dc, run->font_handle->font);

      // B is the absolute width of the ink. A and C are positive for padding and negative for overhang
      // Advance is the absolute width plus padding minus the absolute overhang on both sides.
      int advance = run->abc.abcB + run->abc.abcA + run->abc.abcC;
      auto result = ScriptTextOut(dc, run->font_handle->cache, cursor_x, cursor_y, 0, NULL, &run->analysis, NULL, 0, run->glyphs, run->glyph_count, run->advances, NULL, run->g_offsets);
      if(result != 0) {
        printf("Error: ScriptTextOut failed %x\n", result);
      }
      cursor_x += advance;
    }
    cursor_y += max_line_height;
  }

  *cursor_y_ptr = cursor_y;
}

void free_layout(LayoutParagraph *l){
  auto next_line = l->first_line;
  for(auto line=next_line; line; line = next_line){
    next_line = line->next;
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
    free(line->embedding_levels);
    free(line->visual_to_logical);
    free(line->logical_to_visual);
    free(line);
  }
  free(l);
}

