#include "../content/syntax_highlighter.h"

#include <algorithm>

#include <gtest/gtest.h>

using ai_overlay::SyntaxHighlighter;
using ai_overlay::SyntaxTokenKind;

TEST(SyntaxHighlighterTest, RecognizesKeyword) {
  const auto tokens = SyntaxHighlighter::TokenizeLine(L"if (x) {", L"cpp");
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens[0].text, L"if");
  EXPECT_EQ(tokens[0].kind, SyntaxTokenKind::kKeyword);
}

TEST(SyntaxHighlighterTest, RecognizesStringLiteral) {
  const auto tokens = SyntaxHighlighter::TokenizeLine(L"x = \"hello\";", L"cpp");
  const auto it = std::find_if(tokens.begin(), tokens.end(), [](const auto& t) {
    return t.kind == SyntaxTokenKind::kString;
  });
  ASSERT_NE(it, tokens.end());
  EXPECT_EQ(it->text, L"\"hello\"");
}

TEST(SyntaxHighlighterTest, RecognizesFunctionCallHeuristic) {
  const auto tokens = SyntaxHighlighter::TokenizeLine(L"print(1)", L"python");
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens[0].text, L"print");
  EXPECT_EQ(tokens[0].kind, SyntaxTokenKind::kFunction);
}

TEST(SyntaxHighlighterTest, UsesHashCommentForPython) {
  const auto tokens = SyntaxHighlighter::TokenizeLine(L"# a comment", L"python");
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, SyntaxTokenKind::kComment);
}

TEST(SyntaxHighlighterTest, UsesSlashSlashCommentForCpp) {
  const auto tokens = SyntaxHighlighter::TokenizeLine(L"// a comment", L"cpp");
  ASSERT_EQ(tokens.size(), 1u);
  EXPECT_EQ(tokens[0].kind, SyntaxTokenKind::kComment);
}

TEST(SyntaxHighlighterTest, RecognizesNumberLiteral) {
  const auto tokens = SyntaxHighlighter::TokenizeLine(L"x = 42", L"cpp");
  const auto it = std::find_if(tokens.begin(), tokens.end(), [](const auto& t) {
    return t.kind == SyntaxTokenKind::kNumber;
  });
  ASSERT_NE(it, tokens.end());
  EXPECT_EQ(it->text, L"42");
}

TEST(SyntaxHighlighterTest, EmptyLineProducesNoTokens) {
  EXPECT_TRUE(SyntaxHighlighter::TokenizeLine(L"", L"cpp").empty());
}
