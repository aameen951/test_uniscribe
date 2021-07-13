#define COBJMACROS
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE

#include "my_std.h"

#define ENABLE_SANITY_CHECKS 1

#define ENABLE_MEMORY_LEAK_HOOK
#include "memory_leak.h"
#include "program.cpp"

void win32_render(HWND window, HDC window_dc){
  auto client_rect = RECT{};
  GetClientRect(window, &client_rect);
  auto w = client_rect.right - client_rect.left;
  auto h = client_rect.bottom - client_rect.top;

  // Create a memory device context with a monochrome 1by1 pixels bitmap.
  auto dc = CreateCompatibleDC(window_dc);

  auto bitmap = CreateCompatibleBitmap(
    window_dc, // Use the window HDC to get a bitmap with the correct color format.
    w, h
  );
  SelectObject(dc, bitmap);

  render(window, dc);

  BitBlt(window_dc,
    client_rect.left,
    client_rect.top,
    w,
    h,
    dc,
    0,
    0,
    SRCCOPY
  );

  DeleteObject(bitmap);
  DeleteDC(dc);
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
        case VK_UP: {
          pos_y += GetKeyState(VK_SHIFT) & 0x8000 ? 10 : 1;
        }break;
        case VK_DOWN: {
          pos_y -= GetKeyState(VK_SHIFT) & 0x8000 ? 10 : 1;
        }break;
      }
      InvalidateRect(window, NULL, FALSE);
      UpdateWindow(window);
    }break;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(window, &ps);
      win32_render(window, dc);
      EndPaint(window, &ps);
    } break;

    default:
    {
      Result = DefWindowProcW(window, message, w_param, l_param);
    } break;
  }

  return Result;
}

int main() {
  WNDCLASSEXW window_class = {};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = &window_proc;
  window_class.hInstance = GetModuleHandleW(NULL);
  window_class.hIcon = LoadIconW(NULL, (LPWSTR)IDI_APPLICATION);
  window_class.hCursor = LoadCursorW(NULL, (LPWSTR)IDC_ARROW);
  window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  window_class.lpszClassName = L"test-uniscribe-window";

  HWND window = {};
  if(RegisterClassExW(&window_class))
  {
    DWORD ExStyle = WS_EX_APPWINDOW;
    window = CreateWindowExW(
      ExStyle, window_class.lpszClassName, L"Test Uniscribe", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
      0, 0, window_class.hInstance, 0
    );
  }

  ShowWindow(window, SW_SHOW);

  init(window);

  MSG Message;
  while(true) {
    while(PeekMessageW(&Message, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&Message);
      DispatchMessageW(&Message);
    }

    if(!IsWindow(window))break;

    auto dc = GetDC(window);
    win32_render(window, dc);
    ReleaseDC(window, dc);
  }

  close(window);

  dump_non_free_memory();
}
