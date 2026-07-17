import 'package:ai_overlay_assistant/models/conversation.dart' show ProviderKind;

/// FR-7: prompt templates support CRUD + variable substitution.
/// Variables are written as `{{variable_name}}` in [body].
class PromptTemplate {
  const PromptTemplate({
    required this.id,
    required this.name,
    required this.body,
    this.variableNames = const [],
  });

  final String id;
  final String name;
  final String body;
  final List<String> variableNames;

  String render(Map<String, String> values) {
    var result = body;
    for (final entry in values.entries) {
      result = result.replaceAll('{{${entry.key}}}', entry.value);
    }
    return result;
  }

  PromptTemplate copyWith({
    String? id,
    String? name,
    String? body,
    List<String>? variableNames,
  }) {
    return PromptTemplate(
      id: id ?? this.id,
      name: name ?? this.name,
      body: body ?? this.body,
      variableNames: variableNames ?? this.variableNames,
    );
  }
}

/// Non-secret provider configuration. The API key itself never lives on
/// this model — it's looked up from SecureKeyStorage by [id] at call
/// time (SEC-1).
class ProviderConfig {
  const ProviderConfig({
    required this.id,
    required this.kind,
    required this.defaultModel,
    this.ollamaEndpoint,
  });

  final String id;
  final ProviderKind kind;
  final String defaultModel;

  /// Only set when [kind] is [ProviderKind.ollama].
  final String? ollamaEndpoint;

  ProviderConfig copyWith({
    String? id,
    ProviderKind? kind,
    String? defaultModel,
    String? ollamaEndpoint,
  }) {
    return ProviderConfig(
      id: id ?? this.id,
      kind: kind ?? this.kind,
      defaultModel: defaultModel ?? this.defaultModel,
      ollamaEndpoint: ollamaEndpoint ?? this.ollamaEndpoint,
    );
  }
}

enum HotkeyAction { invokeOverlay, captureRegion, toggleClickThrough, quickAsk }

/// FR-4: user-configurable global hotkeys. [modifiers] uses the same
/// bitmask values as `RegisterHotKey`'s fsModifiers parameter so the
/// native HotkeyManager can consume this without translation.
class HotkeyBinding {
  const HotkeyBinding({
    required this.action,
    required this.virtualKeyCode,
    required this.modifiers,
  });

  final HotkeyAction action;
  final int virtualKeyCode;
  final int modifiers;

  HotkeyBinding copyWith({
    HotkeyAction? action,
    int? virtualKeyCode,
    int? modifiers,
  }) {
    return HotkeyBinding(
      action: action ?? this.action,
      virtualKeyCode: virtualKeyCode ?? this.virtualKeyCode,
      modifiers: modifiers ?? this.modifiers,
    );
  }
}

/// Result of an OCR pass over a captured region (FR-19/FR-20).
/// Deliberately does not carry the source bitmap — that's a transient
/// temp file per 02-ARCHITECTURE.md §9, deleted after use unless the
/// user opts to keep it.
class OcrResult {
  const OcrResult({
    required this.id,
    required this.text,
    required this.capturedAt,
  });

  final String id;
  final String text;
  final DateTime capturedAt;
}
