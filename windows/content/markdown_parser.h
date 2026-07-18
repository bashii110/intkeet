#pragma once
// Minimal markdown subset per 02-ARCHITECTURE.md §4/§7 and 01-SRD.md FR-6:
// headings, bold, lists, and monospace fenced code blocks. Not a general
// CommonMark implementation — deliberately scoped to what streamed AI
// responses actually use.
//
// Parses the FULL accumulated text on every call rather than truly
// incrementally tokenizing — for chat-message-length text this is cheap
// (microseconds), and it keeps the parser simple and easy to test
// exhaustively. The perf-relevant part of the Phase 2 gate (dirty-rect
// repaint, not full-window redraw per token) is satisfied one layer up,
// in ContentModel, by diffing the resulting Block list against the
// previous parse and only invalidating blocks that actually changed.

#include <string>
#include <vector>

#include "block.h"

namespace ai_overlay {

class MarkdownParser {
 public:
  static std::vector<Block> Parse(const std::wstring& text);

 private:
  static std::vector<InlineRun> ParseInline(const std::wstring& line);
};

}  // namespace ai_overlay
