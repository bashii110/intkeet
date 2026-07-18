#include "content_model.h"

#include <algorithm>

namespace ai_overlay {

void ContentModel::Reset() {
  raw_text_.clear();
  blocks_.clear();
  pending_dirty_.clear();
}

void ContentModel::AppendToken(const std::wstring& token) {
  raw_text_ += token;
  std::vector<Block> new_blocks = MarkdownParser::Parse(raw_text_);

  const size_t compare_count = std::min(new_blocks.size(), blocks_.size());
  for (size_t i = 0; i < compare_count; ++i) {
    if (!new_blocks[i].ContentEquals(blocks_[i])) {
      pending_dirty_.push_back(static_cast<int>(i));
    }
  }
  // Any block beyond the previous block count is new — always dirty.
  for (size_t i = compare_count; i < new_blocks.size(); ++i) {
    pending_dirty_.push_back(static_cast<int>(i));
  }

  blocks_ = std::move(new_blocks);
}

std::vector<int> ContentModel::TakeDirtyBlockIndices() {
  std::vector<int> result;
  std::swap(result, pending_dirty_);
  // Diffing can produce duplicate indices across repeated AppendToken
  // calls between two Draw() passes — de-dupe so the renderer doesn't
  // redundantly re-layout the same block twice in one paint.
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

}  // namespace ai_overlay
