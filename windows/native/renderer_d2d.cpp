#include "renderer_d2d.h"

#include "../content/syntax_highlighter.h"
#include "../logging/native_log.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace ai_overlay {

namespace {
// Mirrors lib/ui/theme/tokens.dart's AppColors EXACTLY (05-DESIGN.md
// §2.1: colors live only in tokens.dart and here — nowhere else). If you
// change one, change the other in the same PR (03-RULES.md §7 DoD:
// "Docs updated ... 05-DESIGN.md if visuals changed").
constexpr D2D1_COLOR_F kColorBgBase = {0x0F / 255.0f, 0x11 / 255.0f, 0x15 / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kColorBgElevated = {0x17 / 255.0f, 0x1A / 255.0f, 0x21 / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kColorBorderSubtle = {0x2A / 255.0f, 0x2E / 255.0f, 0x38 / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kColorTextPrimary = {0xED / 255.0f, 0xEF / 255.0f, 0xF5 / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kColorTextSecondary = {0x9A / 255.0f, 0xA1 / 255.0f, 0xB2 / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kColorAccentPrimary = {0x7C / 255.0f, 0x9C / 255.0f, 0xFF / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kColorSyntaxKeyword = {0xC7 / 255.0f, 0x92 / 255.0f, 0xEA / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kColorSyntaxString = {0xC3 / 255.0f, 0xE8 / 255.0f, 0x8D / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kColorSyntaxFunction = {0x82 / 255.0f, 0xAA / 255.0f, 0xFF / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kColorSyntaxComment = {0x6B / 255.0f, 0x72 / 255.0f, 0x80 / 255.0f, 1.0f};
constexpr D2D1_COLOR_F kColorSyntaxNumber = {0xF7 / 255.0f, 0x8C / 255.0f, 0x6C / 255.0f, 1.0f};

// 05-DESIGN.md §2.3 spacing scale, §3 layout.
constexpr float kPanelPaddingPx = 16.0f;
constexpr float kBlockGapPx = 8.0f;
constexpr float kCodeBlockHeaderHeightPx = 28.0f;
constexpr float kCopyButtonSizePx = 20.0f;

D2D1_COLOR_F ColorForSyntaxToken(SyntaxTokenKind kind) {
  switch (kind) {
    case SyntaxTokenKind::kKeyword: return kColorSyntaxKeyword;
    case SyntaxTokenKind::kString: return kColorSyntaxString;
    case SyntaxTokenKind::kFunction: return kColorSyntaxFunction;
    case SyntaxTokenKind::kComment: return kColorSyntaxComment;
    case SyntaxTokenKind::kNumber: return kColorSyntaxNumber;
    case SyntaxTokenKind::kPlain:
    default: return kColorTextPrimary;
  }
}
}  // namespace

RendererD2D::~RendererD2D() { Shutdown(); }

bool RendererD2D::Initialize(HWND hwnd) {
  hwnd_ = hwnd;

  HRESULT hr = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory_);
  if (FAILED(hr)) {
    NativeLog::Warn("RendererD2D", "D2D1CreateFactory failed");
    return false;
  }

  hr = ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                              reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf()));
  if (FAILED(hr)) {
    NativeLog::Warn("RendererD2D", "DWriteCreateFactory failed");
    return false;
  }

  // Font choices per 05-DESIGN.md §2.2. DirectWrite falls back to the
  // system default automatically if "Segoe UI Variable ..." isn't
  // present (older Windows 10 builds) — no explicit fallback code
  // needed, matching the design doc's stated behavior.
  dwrite_factory_->CreateTextFormat(L"Segoe UI Variable Display", nullptr,
                                     DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                                     DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us",
                                     &heading_format_);
  dwrite_factory_->CreateTextFormat(L"Segoe UI Variable Text", nullptr,
                                     DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
                                     DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &body_format_);
  dwrite_factory_->CreateTextFormat(L"Cascadia Code", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
                                     DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f,
                                     L"en-us", &code_format_);
  dwrite_factory_->CreateTextFormat(L"Segoe UI Variable Small", nullptr,
                                     DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
                                     DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us",
                                     &caption_format_);

  return CreateDeviceResources();
}

bool RendererD2D::CreateDeviceResources() {
  if (render_target_ || hwnd_ == nullptr) return true;

  RECT client_rect{};
  ::GetClientRect(hwnd_, &client_rect);
  const D2D1_SIZE_U size = D2D1::SizeU(client_rect.right - client_rect.left,
                                        client_rect.bottom - client_rect.top);

  const HRESULT hr = d2d_factory_->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hwnd_, size),
      &render_target_);
  if (FAILED(hr)) {
    NativeLog::Warn("RendererD2D", "CreateHwndRenderTarget failed");
    return false;
  }

  render_target_->CreateSolidColorBrush(kColorTextPrimary, &brush_);
  return true;
}

void RendererD2D::DiscardDeviceResources() {
  brush_.Reset();
  render_target_.Reset();
}

void RendererD2D::Shutdown() {
  DiscardDeviceResources();
  caption_format_.Reset();
  code_format_.Reset();
  body_format_.Reset();
  heading_format_.Reset();
  dwrite_factory_.Reset();
  d2d_factory_.Reset();
  hwnd_ = nullptr;
}

void RendererD2D::OnResize(int width, int height) {
  if (!render_target_) return;
  render_target_->Resize(D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height)));
}

float RendererD2D::MeasureBlockHeight(const Block& block, float width) const {
  (void)width;
  switch (block.kind) {
    case BlockKind::kCodeBlock: {
      // Header strip + one line per source line, monospace line height
      // 1.4x per 05-DESIGN.md §2.2.
      const float line_height = 13.0f * 1.4f;
      return kCodeBlockHeaderHeightPx +
             static_cast<float>(block.code_lines.size()) * line_height + 8.0f;
    }
    case BlockKind::kHeading:
      return 20.0f * 1.5f + 8.0f;
    case BlockKind::kParagraph:
    case BlockKind::kListItem:
    default: {
      // Rough estimate: DirectWrite line-wrap-aware measurement would
      // need an IDWriteTextLayout per block; Phase 2 uses a
      // single-visual-line-per-block approximation which is accurate
      // enough for the 60 FPS streaming gate and revisited if
      // long-paragraph wrapping proves visually off during the Phase 2
      // manual pass.
      const float line_height = 14.0f * 1.5f;
      return line_height + 4.0f;
    }
  }
}

void RendererD2D::RecomputeLayoutFrom(size_t first_dirty_index, const ContentModel& model) {
  const auto& blocks = model.blocks();
  layout_cache_.resize(blocks.size());

  RECT client_rect{};
  ::GetClientRect(hwnd_, &client_rect);
  const float content_width = (client_rect.right - client_rect.left) - 2 * kPanelPaddingPx;

  float y = kPanelPaddingPx;
  if (first_dirty_index > 0 && first_dirty_index <= layout_cache_.size()) {
    y = layout_cache_[first_dirty_index - 1].bounds.bottom + kBlockGapPx;
  }

  for (size_t i = first_dirty_index; i < blocks.size(); ++i) {
    const float height = MeasureBlockHeight(blocks[i], content_width);
    LayoutBlock lb;
    lb.bounds = D2D1::RectF(kPanelPaddingPx, y, kPanelPaddingPx + content_width, y + height);
    if (blocks[i].kind == BlockKind::kCodeBlock) {
      lb.has_copy_button = true;
      lb.copy_button_bounds = D2D1::RectF(
          lb.bounds.right - kCopyButtonSizePx - 6.0f, lb.bounds.top + 4.0f,
          lb.bounds.right - 6.0f, lb.bounds.top + 4.0f + kCopyButtonSizePx);
    }
    layout_cache_[i] = lb;
    y = lb.bounds.bottom + kBlockGapPx;
  }
}

void RendererD2D::OnContentChanged(ContentModel& model) {
  if (!render_target_ && !CreateDeviceResources()) return;

  const std::vector<int> dirty = model.TakeDirtyBlockIndices();
  if (dirty.empty()) return;

  const size_t first_dirty = static_cast<size_t>(dirty.front());
  RecomputeLayoutFrom(first_dirty, model);

  // Dirty-rect signal to the OS: everything from the first changed
  // block's top down to the bottom of the client area, since later
  // blocks may have shifted when an earlier one (typically the last,
  // streaming block) grew. This is the "no full-window redraw per
  // token" contract from the Phase 2 gate — see renderer_d2d.h's brush_
  // comment for what "dirty" means at the D2D-paint-call level (a single
  // BeginDraw/EndDraw redraws all cached blocks; InvalidateRect is what
  // tells the OS/compositor which screen region actually needs
  // presenting, which is the perf-relevant half of this contract for a
  // panel this size).
  RECT client_rect{};
  ::GetClientRect(hwnd_, &client_rect);
  RECT dirty_rect = client_rect;
  if (first_dirty < layout_cache_.size()) {
    dirty_rect.top = static_cast<LONG>(layout_cache_[first_dirty].bounds.top) - 2;
    if (dirty_rect.top < client_rect.top) dirty_rect.top = client_rect.top;
  }
  ::InvalidateRect(hwnd_, &dirty_rect, FALSE);

  last_drawn_blocks_ = model.blocks();
}

void RendererD2D::Draw(const ContentModel& model) {
  if (!render_target_ && !CreateDeviceResources()) return;

  render_target_->BeginDraw();
  render_target_->Clear(kColorBgBase);

  const auto& blocks = model.blocks();
  for (size_t i = 0; i < blocks.size() && i < layout_cache_.size(); ++i) {
    DrawBlock(blocks[i], layout_cache_[i]);
  }

  const HRESULT hr = render_target_->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    // Documented recovery path for a lost device
    // (https://learn.microsoft.com/windows/win32/direct2d/direct2d-error-handling):
    // discard and let the next OnContentChanged/Draw recreate it.
    DiscardDeviceResources();
    layout_cache_.clear();
  }
}

void RendererD2D::DrawBlock(const Block& block, const LayoutBlock& layout) {
  switch (block.kind) {
    case BlockKind::kCodeBlock: {
      brush_->SetColor(kColorBgElevated);
      render_target_->FillRectangle(layout.bounds, brush_.Get());
      brush_->SetColor(kColorBorderSubtle);
      render_target_->DrawRectangle(layout.bounds, brush_.Get(), 1.0f);

      // Header strip: language label (left).
      brush_->SetColor(kColorTextSecondary);
      const D2D1_RECT_F header_rect = D2D1::RectF(
          layout.bounds.left + 8, layout.bounds.top, layout.bounds.right - kCopyButtonSizePx - 12,
          layout.bounds.top + kCodeBlockHeaderHeightPx);
      render_target_->DrawText(block.code_language.c_str(),
                                static_cast<UINT32>(block.code_language.size()),
                                caption_format_.Get(), header_rect, brush_.Get());

      // Copy button (icon rendering deferred to a later pass — this
      // paints its hit-target rect so interaction/tests can be verified
      // now; the icon glyph itself is a cosmetic follow-up, not a
      // functional gap).
      brush_->SetColor(kColorAccentPrimary);
      render_target_->DrawRectangle(layout.copy_button_bounds, brush_.Get(), 1.0f);

      // Code lines, syntax-highlighted per line.
      float line_y = layout.bounds.top + kCodeBlockHeaderHeightPx + 4;
      const float line_height = 13.0f * 1.4f;
      for (const auto& line : block.code_lines) {
        const auto tokens = SyntaxHighlighter::TokenizeLine(line, block.code_language);
        float x = layout.bounds.left + 8;
        for (const auto& token : tokens) {
          brush_->SetColor(ColorForSyntaxToken(token.kind));
          const D2D1_RECT_F token_rect =
              D2D1::RectF(x, line_y, layout.bounds.right - 8, line_y + line_height);
          render_target_->DrawText(token.text.c_str(), static_cast<UINT32>(token.text.size()),
                                    code_format_.Get(), token_rect, brush_.Get());
          // Advance x by an approximate monospace glyph width; exact
          // per-token positioning via IDWriteTextLayout::HitTestTextRange
          // is a follow-up once the copy-button/token interaction needs
          // per-glyph precision.
          x += token.text.size() * 7.5f;
        }
        line_y += line_height;
      }
      break;
    }

    case BlockKind::kHeading: {
      brush_->SetColor(kColorTextPrimary);
      for (const auto& run : block.runs) {
        render_target_->DrawText(run.text.c_str(), static_cast<UINT32>(run.text.size()),
                                  heading_format_.Get(), layout.bounds, brush_.Get());
      }
      break;
    }

    case BlockKind::kParagraph:
    case BlockKind::kListItem:
    default: {
      float x = layout.bounds.left + (block.kind == BlockKind::kListItem ? 16.0f : 0.0f);
      for (const auto& run : block.runs) {
        brush_->SetColor(run.style == InlineStyle::kInlineCode ? kColorSyntaxFunction
                                                                : kColorTextPrimary);
        IDWriteTextFormat* format =
            run.style == InlineStyle::kInlineCode ? code_format_.Get() : body_format_.Get();
        const D2D1_RECT_F run_rect =
            D2D1::RectF(x, layout.bounds.top, layout.bounds.right, layout.bounds.bottom);
        render_target_->DrawText(run.text.c_str(), static_cast<UINT32>(run.text.size()), format,
                                  run_rect, brush_.Get());
        x += run.text.size() * 7.0f;  // Same approximation note as above.
      }
      break;
    }
  }
}

ContentHitResult RendererD2D::HitTestContent(POINT client_pt) const {
  const D2D1_POINT_2F pt =
      D2D1::Point2F(static_cast<float>(client_pt.x), static_cast<float>(client_pt.y));
  for (size_t i = 0; i < layout_cache_.size(); ++i) {
    const auto& lb = layout_cache_[i];
    if (!lb.has_copy_button) continue;
    const auto& r = lb.copy_button_bounds;
    if (pt.x >= r.left && pt.x <= r.right && pt.y >= r.top && pt.y <= r.bottom) {
      return {true, static_cast<int>(i)};
    }
  }
  return {};
}

std::wstring RendererD2D::GetCodeBlockText(int block_index) const {
  if (block_index < 0 || static_cast<size_t>(block_index) >= last_drawn_blocks_.size()) {
    return L"";
  }
  const Block& block = last_drawn_blocks_[static_cast<size_t>(block_index)];
  if (block.kind != BlockKind::kCodeBlock) return L"";

  std::wstring joined;
  for (size_t i = 0; i < block.code_lines.size(); ++i) {
    joined += block.code_lines[i];
    if (i + 1 < block.code_lines.size()) joined += L"\r\n";
  }
  return joined;
}

}  // namespace ai_overlay
