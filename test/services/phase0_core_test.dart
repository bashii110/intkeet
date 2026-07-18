import 'package:ai_overlay_assistant/models/config_models.dart';
import 'package:ai_overlay_assistant/models/conversation.dart';
import 'package:ai_overlay_assistant/services/ai/ai_provider.dart';
import 'package:ai_overlay_assistant/services/logging/app_logger.dart';
import 'package:ai_overlay_assistant/services/storage/secure_key_storage.dart';
import 'package:flutter_test/flutter_test.dart';

class _CapturingSink implements LogSink {
  final records = <LogRecord>[];
  @override
  void write(LogRecord record) => records.add(record);
}

void main() {
  group('AppLogger', () {
    test('respects minLevel', () {
      final sink = _CapturingSink();
      final logger = AppLogger('test', sinks: [sink], minLevel: LogLevel.warn);
      logger.debug('should be dropped');
      logger.info('also dropped');
      logger.warn('kept');
      expect(sink.records, hasLength(1));
      expect(sink.records.single.level, LogLevel.warn);
    });
  });

  group('redactForLog', () {
    test('never returns the full value for long strings', () {
      final redacted = redactForLog('sk-superSecretApiKeyValue1234567890');
      expect(redacted, isNot(contains('SecretApiKeyValue')));
      expect(redacted, startsWith('sk-superSecr'));
    });

    test('fully masks short values', () {
      expect(redactForLog('short'), '*****');
    });
  });

  group('InMemorySecureKeyStorage', () {
    test('write/read/delete round trip', () async {
      final storage = InMemorySecureKeyStorage();
      await storage.write(key: 'openai', value: 'sk-abc123');
      expect(await storage.read(key: 'openai'), 'sk-abc123');

      await storage.delete(key: 'openai');
      expect(await storage.read(key: 'openai'), isNull);
    });
  });

  group('PromptTemplate', () {
    test('renders variable substitution', () {
      const template = PromptTemplate(
        id: 't1',
        name: 'Summarize',
        body: 'Summarize this {{content_type}} in {{tone}} tone.',
        variableNames: ['content_type', 'tone'],
      );
      final rendered = template.render({'content_type': 'article', 'tone': 'formal'});
      expect(rendered, 'Summarize this article in formal tone.');
    });
  });

  group('FakeAiProvider', () {
    test('streams tokens then a final marker', () async {
      final provider = FakeAiProvider(ProviderKind.anthropic);
      final message = Message(
        id: 'm1',
        conversationId: 'c1',
        role: MessageRole.user,
        content: 'hi',
        createdAt: DateTime(2026, 1, 1),
      );

      final tokens = await provider
          .streamChat(messages: [message], model: 'claude-sonnet-5')
          .toList();

      expect(tokens.last.isFinal, isTrue);
      expect(tokens.where((t) => !t.isFinal).map((t) => t.text).join(), 'Hello, world.');
    });
  });
}
