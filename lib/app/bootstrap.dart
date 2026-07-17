import 'package:ai_overlay_assistant/providers/app_providers.dart';
import 'package:ai_overlay_assistant/services/overlay/overlay_bridge.dart';
import 'package:ai_overlay_assistant/services/storage/local_database.dart';
import 'package:ai_overlay_assistant/services/storage/secure_key_storage.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Phase 0 override set. `OverlayManager` (native) doesn't exist yet, so
/// this wires in-memory/fake implementations of every service so the
/// Flutter host window boots and is fully testable in isolation.
///
/// As each phase lands its real implementation, remove the corresponding
/// override here and construct the real thing instead — no other file
/// should need to change.
List<Override> phase0Overrides() {
  return [
    localDatabaseProvider.overrideWithValue(InMemoryLocalDatabase()),
    secureKeyStorageProvider.overrideWithValue(InMemorySecureKeyStorage()),
    overlayBridgeProvider.overrideWithValue(FakeOverlayBridge()),
  ];
}

void bootstrap() {
  runApp(
    ProviderScope(
      overrides: phase0Overrides(),
      child: const AiOverlayAssistantApp(),
    ),
  );
}

class AiOverlayAssistantApp extends StatelessWidget {
  const AiOverlayAssistantApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AI Overlay Assistant',
      debugShowCheckedModeBanner: false,
      // Dark theme is the only theme for v1 (03-RULES.md §5) — real
      // ColorScheme comes from ui/theme/tokens.dart (05-DESIGN.md §2.1)
      // once the Flutter theme layer is built in Phase 3. Placeholder
      // dark theme only for Phase 0.
      theme: ThemeData.dark(useMaterial3: true),
      home: const _Phase0PlaceholderHome(),
    );
  }
}

/// Blank-but-real host window content for the Phase 0 gate. This is the
/// Flutter "control panel" window (02-ARCHITECTURE.md §1) — the overlay
/// itself is a separate native window that doesn't exist until Phase 1.
class _Phase0PlaceholderHome extends StatelessWidget {
  const _Phase0PlaceholderHome();

  @override
  Widget build(BuildContext context) {
    return const Scaffold(
      body: Center(
        child: Text('AI Overlay Assistant — Phase 0 scaffold'),
      ),
    );
  }
}
