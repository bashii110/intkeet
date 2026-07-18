#include "../content/markdown_parser.h"

#include <gtest/gtest.h>

using ai_overlay::BlockKind;
using ai_overlay::InlineStyle;
using ai_overlay::MarkdownParser;

TEST(MarkdownParserTest, EmptyTextProducesNoBlocks) {
  EXPECT_TRUE(MarkdownParser::Parse(L"").empty());
}

TEST(MarkdownParserTest, ParsesHeading) {
  const auto blocks = MarkdownParser::Parse(L"## Section title");
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].kind, BlockKind::kHeading);
  EXPECT_EQ(blocks[0].level, 2);
  ASSERT_EQ(blocks[0].runs.size(), 1u);
  EXPECT_EQ(blocks[0].runs[0].text, L"Section title");
}

TEST(MarkdownParserTest, ParsesBoldAndInlineCodeSpans) {
  const auto blocks = MarkdownParser::Parse(L"This is **bold** and `code`.");
  ASSERT_EQ(blocks.size(), 1u);
  const auto& runs = blocks[0].runs;
  ASSERT_EQ(runs.size(), 5u);
  EXPECT_EQ(runs[0].text, L"This is ");
  EXPECT_EQ(runs[0].style, InlineStyle::kPlain);
  EXPECT_EQ(runs[1].text, L"bold");
  EXPECT_EQ(runs[1].style, InlineStyle::kBold);
  EXPECT_EQ(runs[3].text, L"code");
  EXPECT_EQ(runs[3].style, InlineStyle::kInlineCode);
}

TEST(MarkdownParserTest, ParsesListItems) {
  const auto blocks = MarkdownParser::Parse(L"- first\n- second\n1. third");
  ASSERT_EQ(blocks.size(), 3u);
  for (const auto& b : blocks) {
    EXPECT_EQ(b.kind, BlockKind::kListItem);
  }
  EXPECT_EQ(blocks[0].runs[0].text, L"first");
  EXPECT_EQ(blocks[2].runs[0].text, L"third");
}

TEST(MarkdownParserTest, ParsesFencedCodeBlockWithLanguage) {
  const auto blocks = MarkdownParser::Parse(L"```dart\nvoid main() {}\nprint(1);\n```");
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].kind, BlockKind::kCodeBlock);
  EXPECT_EQ(blocks[0].code_language, L"dart");
  ASSERT_EQ(blocks[0].code_lines.size(), 2u);
  EXPECT_EQ(blocks[0].code_lines[0], L"void main() {}");
}

TEST(MarkdownParserTest, UnterminatedFenceStillProducesPartialCodeBlock) {
  // Streaming scenario: text arrives mid-code-block before the closing
  // fence has streamed in yet (05-DESIGN.md §7 "partial message kept").
  const auto blocks = MarkdownParser::Parse(L"```python\nprint('partial'");
  ASSERT_EQ(blocks.size(), 1u);
  EXPECT_EQ(blocks[0].kind, BlockKind::kCodeBlock);
  EXPECT_EQ(blocks[0].code_lines.size(), 1u);
}

TEST(MarkdownParserTest, MixedParagraphsAndCodeBlocksInOneDocument) {
  const auto blocks =
      MarkdownParser::Parse(L"# Title\nSome text.\n```js\nconsole.log(1);\n```\nMore text.");
  ASSERT_EQ(blocks.size(), 4u);
  EXPECT_EQ(blocks[0].kind, BlockKind::kHeading);
  EXPECT_EQ(blocks[1].kind, BlockKind::kParagraph);
  EXPECT_EQ(blocks[2].kind, BlockKind::kCodeBlock);
  EXPECT_EQ(blocks[3].kind, BlockKind::kParagraph);
}
