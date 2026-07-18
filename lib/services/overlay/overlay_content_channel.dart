import 'dart:ffi' as ffi;
import 'dart:io';

import 'package:ai_overlay_assistant/services/logging/app_logger.dart';
import 'package:ffi/ffi.dart' as pkg_ffi;

// Must match the extern "C" signatures in windows/ffi/overlay_api.h.
typedef _AppendTokenNative = ffi.Int32 Function(ffi.Pointer<pkg_ffi.Utf16>);
typedef _AppendTokenDart = int Function(ffi.Pointer<pkg_ffi.Utf16>);
typedef _ResetContentNative = ffi.Void Function();
typedef _ResetContentDart = void Function();

/// High-frequency content streaming path (02-ARCHITECTURE.md §5, Phase
/// 2): bypasses OverlayBridge's MethodChannel/JSON round trip since this
/// is called once per streamed token, potentially 50+ times/sec
/// (Phase 2 gate).
///
/// Deliberately a separate interface from [OverlayBridge] rather than an
/// addition to it — the discrete calls (show/hide/setBounds/...) and the
/// hot streaming path have very different call-frequency and
/// error-handling shapes, and conflating them would pressure both into
/// the wrong contract.
abstract class OverlayContentChannel {
  /// Appends one streamed token to the overlay's current message.
  /// Returns false if the native side reports no active overlay (never
  /// throws for that case — callers stream tokens far too often for
  /// exceptions to be the right signal; log and continue is correct here,
  /// per 03-RULES.md §2 "no bare catch" doesn't apply since this isn't
  /// swallowing an exception, it's a documented non-exceptional return).
  bool appendToken(String token);

  void resetContent();
}

class FakeOverlayContentChannel implements OverlayContentChannel {
  final List<String> appendedTokens = [];
  bool resetCallCount = false;

  @override
  bool appendToken(String token) {
    appendedTokens.add(token);
    return true;
  }

  @override
  void resetContent() {
    resetCallCount = true;
    appendedTokens.clear();
  }
}

class WindowsOverlayContentChannel implements OverlayContentChannel {
  WindowsOverlayContentChannel({ffi.DynamicLibrary? nativeLibraryOverride})
      : _log = AppLogger('WindowsOverlayContentChannel') {
    if (!Platform.isWindows && nativeLibraryOverride == null) {
      // Constructible (not usable) on non-Windows dev machines so
      // `flutter analyze`/non-Windows unit tests don't fail on import.
      _appendToken = null;
      _resetContent = null;
      return;
    }
    final lib = nativeLibraryOverride ?? ffi.DynamicLibrary.process();
    _appendToken = lib.lookupFunction<_AppendTokenNative, _AppendTokenDart>(
        'AiOverlay_AppendContentToken');
    _resetContent = lib.lookupFunction<_ResetContentNative, _ResetContentDart>(
        'AiOverlay_ResetContent');
  }

  final AppLogger _log;
  _AppendTokenDart? _appendToken;
  _ResetContentDart? _resetContent;

  @override
  bool appendToken(String token) {
    final fn = _appendToken;
    if (fn == null) return false;

    final nativeStr = token.toNativeUtf16();
    try {
      final result = fn(nativeStr);
      if (result == 0) {
        _log.warn('AiOverlay_AppendContentToken reported no active overlay.');
      }
      return result != 0;
    } finally {
      // Native side copies the string synchronously (std::wstring
      // construction in AiOverlay_AppendContentToken) before returning,
      // so freeing immediately after the call is safe.
      pkg_ffi.calloc.free(nativeStr);
    }
  }

  @override
  void resetContent() {
    _resetContent?.call();
  }
}
