#pragma once
// Renderer-agnostic representation of a streamed message's content.
// Both renderer_d2d and renderer_webview2 consume the same Block list —
// this is the "same OverlayContentRenderer interface" contract from
// 02-ARCHITECTURE.md §7.

#include <string>
#include <vector>

namespace ai_overlay {

enum class InlineStyle { kPlain, kBold, kInlineCode };

struct InlineRun {
  std::wstring text;
  InlineStyle style = InlineStyle::kPlain;

  bool operator==(const InlineRun& other) const {
    return text == other.text && style == other.style;
  }
  bool operator!=(const InlineRun& other) const { return !(*this == other); }
};

enum class BlockKind { kHeading, kParagraph, kListItem, kCodeBlock };

struct Block {
  BlockKind kind = BlockKind::kParagraph;

  // kHeading: 1-3 (##/###/####-style depth, matching 05-DESIGN.md's
  // header/body/caption type scale rather than full h1-h6).
  // kListItem: nesting depth (0 = top level).
  int level = 0;

  // kHeading / kParagraph / kListItem.
  std::vector<InlineRun> runs;

  // kCodeBlock only:
  std::wstring code_language;
  std::vector<std::wstring> code_lines;
  bool wrap_enabled = false;  // 05-DESIGN.md §4.2 per-block wrap toggle

  bool ContentEquals(const Block& other) const {
    return kind == other.kind && level == other.level && runs == other.runs &&
           code_language == other.code_language && code_lines == other.code_lines &&
           wrap_enabled == other.wrap_enabled;
  }
};

}  // namespace ai_overlay
