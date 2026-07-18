import 'package:ai_overlay_assistant/services/overlay/overlay_content_channel.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('FakeOverlayContentChannel', () {
    test('accumulates appended tokens in order', () {
      final channel = FakeOverlayContentChannel();
      expect(channel.appendToken('Hello'), isTrue);
      expect(channel.appendToken(', '), isTrue);
      expect(channel.appendToken('world.'), isTrue);
      expect(channel.appendedTokens.join(), 'Hello, world.');
    });

    test('resetContent clears accumulated tokens', () {
      final channel = FakeOverlayContentChannel();
      channel.appendToken('draft');
      channel.resetContent();
      expect(channel.appendedTokens, isEmpty);
      expect(channel.resetCallCount, isTrue);
    });
  });

  group('WindowsOverlayContentChannel', () {
    test('appendToken returns false with no native library loaded', () {
      // On non-Windows test runners (this suite's CI), the constructor
      // skips the FFI lookup entirely (see class doc comment) — this
      // pins that fallback contract down so it fails loudly if it ever
      // regresses into throwing instead of returning false.
      final channel = WindowsOverlayContentChannel();
      expect(channel.appendToken('token'), isFalse);
      expect(() => channel.resetContent(), returnsNormally);
    });
  });
}
