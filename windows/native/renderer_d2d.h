#pragma once
// Direct2D + DirectWrite content renderer (02-ARCHITECTURE.md §7,
// "Default. Lowest latency, smallest memory, matches perf goals.").
// Thread-affinity: OverlayThread only — same thread that owns the HWND
// and pumps WM_PAINT.

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>  // Microsoft::WRL::ComPtr — documented COM RAII wrapper:
                          // https://learn.microsoft.com/cpp/cppcx/wrl/comptr-class

#include <vector>

#include "overlay_content_renderer.h"

namespace ai_overlay {

class RendererD2D : public OverlayContentRenderer {
 public:
  RendererD2D() = default;
  ~RendererD2D() override;

  bool Initialize(HWND hwnd) override;
  void Shutdown() override;
  void OnContentChanged(ContentModel& model) override;
  void OnResize(int width, int height) override;
  void RepaintNow(const ContentModel& model) override { Draw(model); }
  ContentHitResult HitTestContent(POINT client_pt) const override;
  std::wstring GetCodeBlockText(int block_index) const override;

  // Called from OverlayWindow's WM_PAINT handler.
  void Draw(const ContentModel& model);

 private:
  struct LayoutBlock {
    D2D1_RECT_F bounds{};
    D2D1_RECT_F copy_button_bounds{};  // Empty (all zero) for non-code blocks.
    bool has_copy_button = false;
  };

  bool CreateDeviceResources();
  void DiscardDeviceResources();
  void RecomputeLayoutFrom(size_t first_dirty_index, const ContentModel& model);
  float MeasureBlockHeight(const Block& block, float width) const;
  void DrawBlock(const Block& block, const LayoutBlock& layout);

  HWND hwnd_ = nullptr;

  Microsoft::WRL::ComPtr<ID2D1Factory> d2d_factory_;
  Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory_;
  Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> render_target_;

  // One brush, recolored per-draw via SetColor rather than one brush per
  // token color — cheaper than allocating ~10 solid-color brushes for a
  // panel this size, and avoids a stale-brush bug if 05-DESIGN.md tokens
  // change (single source of truth stays tokens.dart's mirrored consts,
  // read from theme_colors.h at brush-recolor time).
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;

  Microsoft::WRL::ComPtr<IDWriteTextFormat> heading_format_;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> body_format_;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> code_format_;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> caption_format_;

  std::vector<LayoutBlock> layout_cache_;
  std::vector<Block> last_drawn_blocks_;  // For GetCodeBlockText() lookups.
};

}  // namespace ai_overlay
