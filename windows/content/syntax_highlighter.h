#pragma once
// Lightweight, language-agnostic heuristic highlighter — not a real
// per-language lexer. Good enough for the common case (keywords,
// strings, line comments, numbers) across the languages AI responses
// typically include code in, without shipping a full grammar table per
// language for v1. Token kinds map 1:1 to 05-DESIGN.md §2.1 syntax.*
// color tokens so the renderer never hardcodes a color here.

#include <string>
#include <vector>

namespace ai_overlay {

enum class SyntaxTokenKind { kPlain, kKeyword, kString, kFunction, kComment, kNumber };

struct SyntaxToken {
  std::wstring text;
  SyntaxTokenKind kind = SyntaxTokenKind::kPlain;
};

class SyntaxHighlighter {
 public:
  // `language` is the fence-info-string language tag (e.g. "dart",
  // "cpp", "python") — currently only used to pick the comment style
  // (`#` vs `//`); the keyword set is shared across languages.
  static std::vector<SyntaxToken> TokenizeLine(const std::wstring& line,
                                                const std::wstring& language);
};

}  // namespace ai_overlay
