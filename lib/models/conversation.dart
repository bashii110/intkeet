/// Which AI provider a conversation/message is associated with.
/// FR-8: providers share a common interface so this stays a flat enum.
enum ProviderKind { openAi, anthropic, gemini, ollama }

enum MessageRole { user, assistant, system }

/// A single message in a conversation. Immutable; no public mutable
/// fields (03-RULES.md §2).
class Message {
  const Message({
    required this.id,
    required this.conversationId,
    required this.role,
    required this.content,
    required this.createdAt,
    this.isPartial = false,
    this.providerKind,
  });

  final String id;
  final String conversationId;
  final MessageRole role;
  final String content;
  final DateTime createdAt;

  /// True while a streamed response is still arriving, or was aborted
  /// mid-stream (FR: "clearly abort and save partial message instead" —
  /// 04-PHASES.md Phase 4).
  final bool isPartial;
  final ProviderKind? providerKind;

  Message copyWith({
    String? id,
    String? conversationId,
    MessageRole? role,
    String? content,
    DateTime? createdAt,
    bool? isPartial,
    ProviderKind? providerKind,
  }) {
    return Message(
      id: id ?? this.id,
      conversationId: conversationId ?? this.conversationId,
      role: role ?? this.role,
      content: content ?? this.content,
      createdAt: createdAt ?? this.createdAt,
      isPartial: isPartial ?? this.isPartial,
      providerKind: providerKind ?? this.providerKind,
    );
  }
}

class Conversation {
  const Conversation({
    required this.id,
    required this.title,
    required this.createdAt,
    required this.updatedAt,
    this.defaultProviderKind,
    this.defaultModel,
  });

  final String id;
  final String title;
  final DateTime createdAt;
  final DateTime updatedAt;
  final ProviderKind? defaultProviderKind;
  final String? defaultModel;

  Conversation copyWith({
    String? id,
    String? title,
    DateTime? createdAt,
    DateTime? updatedAt,
    ProviderKind? defaultProviderKind,
    String? defaultModel,
  }) {
    return Conversation(
      id: id ?? this.id,
      title: title ?? this.title,
      createdAt: createdAt ?? this.createdAt,
      updatedAt: updatedAt ?? this.updatedAt,
      defaultProviderKind: defaultProviderKind ?? this.defaultProviderKind,
      defaultModel: defaultModel ?? this.defaultModel,
    );
  }
}
