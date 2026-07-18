#include "syntax_highlighter.h"

#include <cwctype>
#include <unordered_set>

namespace ai_overlay {

namespace {

const std::unordered_set<std::wstring>& KeywordSet() {
  static const std::unordered_set<std::wstring> keywords = {
      L"if", L"else", L"for", L"while", L"return", L"function", L"def",
      L"class", L"const", L"let", L"var", L"public", L"private", L"static",
      L"import", L"from", L"export", L"async", L"await", L"true", L"false",
      L"null", L"nil", L"none", L"void", L"int", L"double", L"string",
      L"bool", L"final", L"required", L"new", L"this", L"self", L"try",
      L"catch", L"throw", L"switch", L"case", L"break", L"continue",
      L"struct", L"enum", L"namespace", L"using", L"template", L"typename",
  };
  return keywords;
}

bool IsIdentifierChar(wchar_t c) { return iswalnum(c) || c == L'_'; }

std::wstring CommentPrefix(const std::wstring& language) {
  static const std::unordered_set<std::wstring> hashCommentLangs = {
      L"python", L"py", L"bash", L"sh", L"yaml", L"yml", L"ruby", L"rb"};
  return hashCommentLangs.count(language) ? L"#" : L"//";
}

}  // namespace

std::vector<SyntaxToken> SyntaxHighlighter::TokenizeLine(const std::wstring& line,
                                                           const std::wstring& language) {
  std::vector<SyntaxToken> tokens;
  const std::wstring comment_prefix = CommentPrefix(language);

  size_t i = 0;
  while (i < line.size()) {
    // Line comment: rest of line.
    if (line.compare(i, comment_prefix.size(), comment_prefix) == 0) {
      tokens.push_back({line.substr(i), SyntaxTokenKind::kComment});
      break;
    }

    // String literal: '...' or "...".
    if (line[i] == L'"' || line[i] == L'\'') {
      const wchar_t quote = line[i];
      size_t j = i + 1;
      while (j < line.size() && line[j] != quote) ++j;
      const size_t end = (j < line.size()) ? j + 1 : j;  // Include closing quote if found.
      tokens.push_back({line.substr(i, end - i), SyntaxTokenKind::kString});
      i = end;
      continue;
    }

    // Number literal.
    if (iswdigit(line[i])) {
      size_t j = i;
      while (j < line.size() && (iswdigit(line[j]) || line[j] == L'.')) ++j;
      tokens.push_back({line.substr(i, j - i), SyntaxTokenKind::kNumber});
      i = j;
      continue;
    }

    // Identifier / keyword / function-call heuristic.
    if (IsIdentifierChar(line[i]) && !iswdigit(line[i])) {
      size_t j = i;
      while (j < line.size() && IsIdentifierChar(line[j])) ++j;
      const std::wstring word = line.substr(i, j - i);

      SyntaxTokenKind kind = SyntaxTokenKind::kPlain;
      if (KeywordSet().count(word)) {
        kind = SyntaxTokenKind::kKeyword;
      } else if (j < line.size() && line[j] == L'(') {
        kind = SyntaxTokenKind::kFunction;
      }
      tokens.push_back({word, kind});
      i = j;
      continue;
    }

    // Everything else (whitespace, punctuation): accumulate as plain
    // text up to the next token boundary.
    size_t j = i;
    while (j < line.size() && !IsIdentifierChar(line[j]) && line[j] != L'"' &&
           line[j] != L'\'' && line.compare(j, comment_prefix.size(), comment_prefix) != 0) {
      ++j;
    }
    if (j == i) ++j;  // Safety: always make forward progress.
    tokens.push_back({line.substr(i, j - i), SyntaxTokenKind::kPlain});
    i = j;
  }

  return tokens;
}

}  // namespace ai_overlay
