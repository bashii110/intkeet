#include "markdown_parser.h"

#include <sstream>

namespace ai_overlay {

namespace {

std::vector<std::wstring> SplitLines(const std::wstring& text) {
  std::vector<std::wstring> lines;
  std::wstringstream stream(text);
  std::wstring line;
  while (std::getline(stream, line)) {
    // std::getline on a wstringstream leaves trailing '\r' on
    // CRLF-normalized input; strip it so line matching (e.g. fence
    // detection) isn't thrown off.
    if (!line.empty() && line.back() == L'\r') line.pop_back();
    lines.push_back(line);
  }
  // If text ends with '\n', getline already produced all lines correctly.
  // If text is empty, `lines` stays empty, which is the correct parse of
  // an empty message (zero blocks).
  return lines;
}

int CountLeadingChar(const std::wstring& s, wchar_t ch) {
  int count = 0;
  while (count < static_cast<int>(s.size()) && s[count] == ch) ++count;
  return count;
}

bool StartsWithListMarker(const std::wstring& line, size_t* content_start) {
  size_t i = 0;
  while (i < line.size() && line[i] == L' ') ++i;
  if (i < line.size() && (line[i] == L'-' || line[i] == L'*') &&
      i + 1 < line.size() && line[i + 1] == L' ') {
    *content_start = i + 2;
    return true;
  }
  // Numbered list: digits followed by '.' and a space.
  size_t j = i;
  while (j < line.size() && iswdigit(line[j])) ++j;
  if (j > i && j + 1 < line.size() && line[j] == L'.' && line[j + 1] == L' ') {
    *content_start = j + 2;
    return true;
  }
  return false;
}

}  // namespace

std::vector<InlineRun> MarkdownParser::ParseInline(const std::wstring& line) {
  std::vector<InlineRun> runs;
  std::wstring plain_buffer;

  auto flush_plain = [&]() {
    if (!plain_buffer.empty()) {
      runs.push_back({plain_buffer, InlineStyle::kPlain});
      plain_buffer.clear();
    }
  };

  size_t i = 0;
  while (i < line.size()) {
    // Bold: **text**
    if (i + 1 < line.size() && line[i] == L'*' && line[i + 1] == L'*') {
      const size_t close = line.find(L"**", i + 2);
      if (close != std::wstring::npos) {
        flush_plain();
        runs.push_back({line.substr(i + 2, close - (i + 2)), InlineStyle::kBold});
        i = close + 2;
        continue;
      }
    }
    // Inline code: `text`
    if (line[i] == L'`') {
      const size_t close = line.find(L'`', i + 1);
      if (close != std::wstring::npos) {
        flush_plain();
        runs.push_back({line.substr(i + 1, close - (i + 1)), InlineStyle::kInlineCode});
        i = close + 1;
        continue;
      }
    }
    plain_buffer += line[i];
    ++i;
  }
  flush_plain();
  return runs;
}

std::vector<Block> MarkdownParser::Parse(const std::wstring& text) {
  std::vector<Block> blocks;
  const std::vector<std::wstring> lines = SplitLines(text);

  bool in_code_block = false;
  Block current_code;

  for (const auto& raw_line : lines) {
    // Fenced code block start/end: ```language
    if (raw_line.rfind(L"```", 0) == 0) {
      if (!in_code_block) {
        in_code_block = true;
        current_code = Block{};
        current_code.kind = BlockKind::kCodeBlock;
        current_code.code_language = raw_line.substr(3);
      } else {
        in_code_block = false;
        blocks.push_back(std::move(current_code));
        current_code = Block{};
      }
      continue;
    }

    if (in_code_block) {
      current_code.code_lines.push_back(raw_line);
      continue;
    }

    if (raw_line.empty()) {
      // Blank line: block separator only, doesn't itself become a block
      // (avoids a stream of empty paragraph blocks between real ones).
      continue;
    }

    const int heading_hashes = CountLeadingChar(raw_line, L'#');
    if (heading_hashes > 0 && heading_hashes < static_cast<int>(raw_line.size()) &&
        raw_line[heading_hashes] == L' ') {
      Block block;
      block.kind = BlockKind::kHeading;
      block.level = heading_hashes;  // 1 = largest, per type.display scale
      block.runs = ParseInline(raw_line.substr(heading_hashes + 1));
      blocks.push_back(std::move(block));
      continue;
    }

    size_t content_start = 0;
    if (StartsWithListMarker(raw_line, &content_start)) {
      Block block;
      block.kind = BlockKind::kListItem;
      block.level = 0;  // Nested-list depth not modeled in v1's parser.
      block.runs = ParseInline(raw_line.substr(content_start));
      blocks.push_back(std::move(block));
      continue;
    }

    Block block;
    block.kind = BlockKind::kParagraph;
    block.runs = ParseInline(raw_line);
    blocks.push_back(std::move(block));
  }

  // Unterminated fence at end of streamed (partial) text: still show the
  // code so far rather than dropping it — matches FR: partial message
  // kept on stream interruption (01-SRD.md/05-DESIGN.md §7).
  if (in_code_block) {
    blocks.push_back(std::move(current_code));
  }

  return blocks;
}

}  // namespace ai_overlay
