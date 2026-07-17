import 'package:ai_overlay_assistant/models/conversation.dart';

/// A single streamed token from a provider response.
class ChatToken {
  const ChatToken(this.text, {this.isFinal = false});
  final String text;
  final bool isFinal;
}

/// Typed exceptions services throw at their boundary (03-RULES.md §2).
/// UI layer catches these and renders the distinct messages required by
/// Phase 4's gate (auth failure, rate limit, network drop, model-not-found).
sealed class AiProviderException implements Exception {
  const AiProviderException(this.message);
  final String message;

  @override
  String toString() => '$runtimeType: $message';
}

class AuthenticationException extends AiProviderException {
  const AuthenticationException(super.message);
}

class RateLimitException extends AiProviderException {
  const RateLimitException(super.message, {this.retryAfter});
  final Duration? retryAfter;
}

class NetworkException extends AiProviderException {
  const NetworkException(super.message);
}

class ModelNotFoundException extends AiProviderException {
  const ModelNotFoundException(super.message);
}

/// FR-8: "Provider adapters share a common interface ... so new
/// providers can be added without touching UI code."
///
/// Implementations for OpenAI, Anthropic, Gemini, and Ollama land in
/// Phase 4 (`services/ai/openai_provider.dart` etc.). Phase 0 only
/// defines the contract plus a fake for tests below.
abstract class AiProvider {
  ProviderKind get kind;

  /// Streams a completion for [messages] under [model]. Implementations
  /// must map transport-level failures onto the typed exceptions above —
  /// never let a raw HTTP/socket exception cross this boundary.
  Stream<ChatToken> streamChat({
    required List<Message> messages,
    required String model,
  });
}

/// Deterministic fake for widget/unit tests exercising the streaming
/// pipeline (ConversationView, OverlayBridge fast path) without a real
/// network call.
class FakeAiProvider implements AiProvider {
  FakeAiProvider(this.kind, {this.tokens = const ['Hello', ', ', 'world', '.']});

  @override
  final ProviderKind kind;
  final List<String> tokens;

  @override
  Stream<ChatToken> streamChat({
    required List<Message> messages,
    required String model,
  }) async* {
    for (final t in tokens) {
      yield ChatToken(t);
    }
    yield const ChatToken('', isFinal: true);
  }
}
