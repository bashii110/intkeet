import 'package:ai_overlay_assistant/models/config_models.dart';

/// Thrown if OCR is requested while the user hasn't granted the
/// first-run consent (FR-20) — this is a consent gate per 03-RULES.md §1
/// rule 4 and must never be silently bypassed by a caller.
class OcrConsentRequiredException implements Exception {
  const OcrConsentRequiredException();

  @override
  String toString() =>
      'OcrConsentRequiredException: OCR requires explicit first-run consent';
}

/// Orchestrates: native region capture -> OCR engine -> [OcrResult].
/// Talks to native capture via the overlay bridge's region-selection
/// event; the OCR pass itself may run via Windows.Media.Ocr or a bundled
/// engine (implementation detail behind this interface).
///
/// Real implementation lands in Phase 5. Phase 0 only defines the
/// contract and the consent-gate exception so callers can be written
/// against it now.
abstract class OcrService {
  bool get hasConsent;
  Future<void> grantConsent();
  Future<void> revokeConsent();

  /// Throws [OcrConsentRequiredException] if [hasConsent] is false.
  Future<OcrResult> captureAndRecognize();
}
