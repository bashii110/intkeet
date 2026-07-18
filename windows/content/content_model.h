#pragma once
// Thread-affinity: touched from OverlayThread only. The FFI fast path
// (ffi/overlay_api.cpp) marshals onto OverlayThread via OverlayManager's
// command queue before calling into this, same as every other
// window-affecting call (02-ARCHITECTURE.md §2).

#include <string>
#include <vector>

#include "block.h"
#include "markdown_parser.h"

namespace ai_overlay {

// Owns the accumulated raw text for one streaming message and the
// parsed Block list derived from it. Diffs each re-parse against the
// previous one so the renderer only has to repaint blocks that actually
// changed — the mechanism behind "dirty-rect based repaint (no
// full-window redraw per token)" in the Phase 2 gate.
class ContentModel {
 public:
  void Reset();

  // Appends `token` to the raw text and re-parses. Cheap for
  // chat-message-length text (see markdown_parser.h for why full
  // re-parse instead of true incremental tokenizing was chosen).
  void AppendToken(const std::wstring& token);

  const std::vector<Block>& blocks() const { return blocks_; }

  // Returns the indices (into blocks()) that changed since the last
  // call to this method, then clears the pending set. A renderer calls
  // this once per paint pass to know which blocks' cached layouts to
  // invalidate/regenerate.
  std::vector<int> TakeDirtyBlockIndices();

 private:
  std::wstring raw_text_;
  std::vector<Block> blocks_;
  std::vector<int> pending_dirty_;
};

}  // namespace ai_overlay
