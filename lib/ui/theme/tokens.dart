import 'package:flutter/widgets.dart';

/// Design tokens from `05-DESIGN.md` §2. This file and
/// `windows/native/renderer_d2d.cpp`'s `kColor*` constants are the ONLY
/// two places a color hex value may appear (05-DESIGN.md §2.1) — never
/// hardcode a hex value in a widget or in another native file.
class AppColors {
  AppColors._();

  static const bgBase = Color(0xFF0F1115);
  static const bgElevated = Color(0xFF171A21);
  // bg.acrylic.tint is ARGB (80% alpha tint), unlike the rest of this
  // table which is opaque — kept as its own constant rather than
  // derived, since the design doc specifies the exact alpha explicitly.
  static const bgAcrylicTint = Color(0xCC12141A);

  static const borderSubtle = Color(0xFF2A2E38);
  static const borderFocus = Color(0xFF7C9CFF);

  static const textPrimary = Color(0xFFEDEFF5);
  static const textSecondary = Color(0xFF9AA1B2);
  static const textDisabled = Color(0xFF5A5F6B);

  static const accentPrimary = Color(0xFF7C9CFF);
  static const accentPrimaryHover = Color(0xFF93AEFF);
  static const accentSuccess = Color(0xFF5FD98A);
  static const accentWarning = Color(0xFFF2B84B);
  static const accentDanger = Color(0xFFF26D6D);

  static const syntaxKeyword = Color(0xFFC792EA);
  static const syntaxString = Color(0xFFC3E88D);
  static const syntaxFunction = Color(0xFF82AAFF);
  static const syntaxComment = Color(0xFF6B7280);
  static const syntaxNumber = Color(0xFFF78C6C);
}

/// Spacing scale (05-DESIGN.md §2.3): 4 / 8 / 12 / 16 / 24 / 32.
class AppSpacing {
  AppSpacing._();
  static const double xs = 4;
  static const double sm = 8;
  static const double md = 12;
  static const double lg = 16;
  static const double xl = 24;
  static const double xxl = 32;
}

class AppRadius {
  AppRadius._();
  static const double panel = 12;
  static const double card = 8;
  static const double codeBlock = 6;
}

/// Motion durations (05-DESIGN.md §2.4). Native's Animator::*Spec()
/// factory methods in windows/native/animation.h must stay numerically
/// identical to these.
class AppMotion {
  AppMotion._();
  static const overlayShow = Duration(milliseconds: 160);
  static const overlayHide = Duration(milliseconds: 120);
  static const autoHideFade = Duration(milliseconds: 400);
  static const hoverState = Duration(milliseconds: 80);
}
