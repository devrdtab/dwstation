#include "MoonPhase.h"
#include "display/Display.h"
#include <math.h>

double normalizeDegrees(double value) {
  value = fmod(value, 360.0);
  if (value < 0.0)
    value += 360.0;
  return value;
}

MoonInfo getMoonPhase(time_t now) {
  const double MOON_DEG_TO_RAD = PI / 180.0;
  const double MOON_RAD_TO_DEG = 180.0 / PI;
  double jd = ((double)now / 86400.0) + 2440587.5;
  double d = jd - 2451543.5;

  // Солнце
  double ws = (282.9404 + 0.0000470935 * d) * MOON_DEG_TO_RAD;
  double es = 0.016709 - 0.000000001151 * d;
  double Ms = (356.0470 + 0.9856002585 * d) * MOON_DEG_TO_RAD;
  double Es = Ms + es * sin(Ms) * (1.0 + es * cos(Ms));
  double xs = cos(Es) - es;
  double ys = sqrt(1.0 - es * es) * sin(Es);
  double vs = atan2(ys, xs);
  double sunLon = normalizeDegrees((vs + ws) * MOON_RAD_TO_DEG);

  // Луна
  double Nm = (125.1228 - 0.0529538083 * d) * MOON_DEG_TO_RAD;
  double im = 5.1454 * MOON_DEG_TO_RAD;
  double wm = (318.0634 + 0.1643573223 * d) * MOON_DEG_TO_RAD;
  double am = 60.2666;
  double em = 0.054900;
  double Mm = (115.3654 + 13.0649929509 * d) * MOON_DEG_TO_RAD;
  double Em = Mm + em * sin(Mm) * (1.0 + em * cos(Mm));
  double xv = am * (cos(Em) - em);
  double yv = am * (sqrt(1.0 - em * em) * sin(Em));
  double vm = atan2(yv, xv);
  double rm = sqrt(xv * xv + yv * yv);
  double xh = rm * (cos(Nm) * cos(vm + wm) - sin(Nm) * sin(vm + wm) * cos(im));
  double yh = rm * (sin(Nm) * cos(vm + wm) + cos(Nm) * sin(vm + wm) * cos(im));
  double moonLon = atan2(yh, xh);

  double Ls = Ms + ws;
  double Lm = Mm + wm + Nm;
  double D = Lm - Ls;
  double F = Lm - Nm;

  double dlon = 0.0;
  dlon += -1.274 * sin(Mm - 2.0 * D);
  dlon += 0.658 * sin(2.0 * D);
  dlon += -0.186 * sin(Ms);
  dlon += -0.059 * sin(2.0 * Mm - 2.0 * D);
  dlon += -0.057 * sin(Mm - 2.0 * D + Ms);
  dlon += 0.053 * sin(Mm + 2.0 * D);
  dlon += 0.046 * sin(2.0 * D - Ms);
  dlon += 0.041 * sin(Mm - Ms);
  dlon += -0.035 * sin(D);
  dlon += -0.031 * sin(Mm + Ms);
  dlon += -0.015 * sin(2.0 * F - 2.0 * D);
  dlon += 0.011 * sin(Mm - 4.0 * D);
  moonLon = normalizeDegrees(moonLon * MOON_RAD_TO_DEG + dlon);

  double elongation = normalizeDegrees(moonLon - sunLon);
  float phase = (float)(elongation / 360.0);

  float illumination =
      (float)((1.0 - cos(elongation * MOON_DEG_TO_RAD)) * 50.0);
  if (illumination < 0.0)
    illumination = 0.0;
  if (illumination > 100.0)
    illumination = 100.0;

  const char *name;
  if (phase < 0.0625 || phase >= 0.9375)
    name = "NEW";
  else if (phase < 0.1875)
    name = "CRES";
  else if (phase < 0.3125)
    name = "1/4";
  else if (phase < 0.4375)
    name = "GIB";
  else if (phase < 0.5625)
    name = "FULL";
  else if (phase < 0.6875)
    name = "GIB";
  else if (phase < 0.8125)
    name = "3/4";
  else
    name = "CRES";

  MoonInfo result;
  result.phase = phase;
  result.illumination = illumination;
  result.name = name;
  return result;
}

void drawMoon(int cx, int cy, int r, float phase) {
  display.fillCircle(cx, cy, r, SSD1306_BLACK);
  display.drawCircle(cx, cy, r, SSD1306_WHITE);

  if (phase < 0.01 || phase > 0.99)
    return;

  if (phase > 0.49 && phase < 0.51) {
    display.fillCircle(cx, cy, r, SSD1306_WHITE);
    display.drawCircle(cx, cy, r, SSD1306_WHITE);
    return;
  }

  double terminator = cos(2.0 * PI * phase);
  for (int y = -r; y <= r; y++) {
    double dy = (double)y;
    double inside = (double)r * (double)r - dy * dy;
    if (inside < 0.0)
      continue;
    int halfWidth = (int)sqrt(inside);
    int xLeft, xRight;
    int terminatorX = (int)(terminator * halfWidth);
    if (phase < 0.5) {
      xLeft = terminatorX;
      xRight = halfWidth;
    } else {
      xLeft = -halfWidth;
      xRight = -terminatorX;
    }
    if (xRight >= xLeft) {
      display.drawFastHLine(cx + xLeft, cy + y, xRight - xLeft + 1,
                            SSD1306_WHITE);
    }
  }
  display.drawCircle(cx, cy, r, SSD1306_WHITE);
}

void drawMoonIndicator(int x, int y, float phase) {
  if (phase < 0.0625 || phase >= 0.9375) {
    display.drawCircle(x + 3, y + 3, 3, SSD1306_WHITE);
    return;
  }
  if (phase >= 0.4375 && phase < 0.5625) {
    display.fillCircle(x + 3, y + 3, 3, SSD1306_WHITE);
    return;
  }
  if (phase < 0.5) {
    display.drawLine(x + 3, y + 7, x + 3, y, SSD1306_WHITE);
    display.drawLine(x + 3, y, x, y + 3, SSD1306_WHITE);
    display.drawLine(x + 3, y, x + 6, y + 3, SSD1306_WHITE);
    return;
  }
  display.drawLine(x + 3, y, x + 3, y + 7, SSD1306_WHITE);
  display.drawLine(x + 3, y + 7, x, y + 4, SSD1306_WHITE);
  display.drawLine(x + 3, y + 7, x + 6, y + 4, SSD1306_WHITE);
}