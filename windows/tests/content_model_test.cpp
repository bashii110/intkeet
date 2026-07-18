#include "../content/content_model.h"

#include <gtest/gtest.h>

using ai_overlay::ContentModel;

TEST(ContentModelTest, FirstTokenMarksBlockZeroDirty) {
  ContentModel model;
  model.AppendToken(L"Hello");
  const auto dirty = model.TakeDirtyBlockIndices();
  ASSERT_EQ(dirty.size(), 1u);
  EXPECT_EQ(dirty[0], 0);
}

TEST(ContentModelTest, DirtyIndicesClearAfterTake) {
  ContentModel model;
  model.AppendToken(L"Hello");
  model.TakeDirtyBlockIndices();
  EXPECT_TRUE(model.TakeDirtyBlockIndices().empty());
}

TEST(ContentModelTest, GrowingTheSameBlockKeepsItDirtyNotEarlierOnes) {
  ContentModel model;
  model.AppendToken(L"# Title\n");
  model.TakeDirtyBlockIndices();

  model.AppendToken(L"Body text");
  const auto dirty = model.TakeDirtyBlockIndices();
  // Only the paragraph block (index 1) changed; the heading (index 0)
  // must NOT be reported dirty again — this is the mechanism the
  // renderer relies on to avoid a full-window redraw per token.
  ASSERT_EQ(dirty.size(), 1u);
  EXPECT_EQ(dirty[0], 1);
}

TEST(ContentModelTest, ResetClearsBlocksAndRawText) {
  ContentModel model;
  model.AppendToken(L"Some content");
  model.Reset();
  EXPECT_TRUE(model.blocks().empty());
  model.AppendToken(L"fresh");
  ASSERT_EQ(model.blocks().size(), 1u);
  EXPECT_EQ(model.blocks()[0].runs[0].text, L"fresh");
}

TEST(ContentModelTest, AppendingWithinACodeBlockKeepsItAsOneDirtyBlock) {
  ContentModel model;
  model.AppendToken(L"```dart\n");
  model.TakeDirtyBlockIndices();

  model.AppendToken(L"void main() {}\n");
  const auto dirty = model.TakeDirtyBlockIndices();
  ASSERT_EQ(dirty.size(), 1u);
  EXPECT_EQ(dirty[0], 0);
  ASSERT_EQ(model.blocks().size(), 1u);
  ASSERT_EQ(model.blocks()[0].code_lines.size(), 1u);
}
