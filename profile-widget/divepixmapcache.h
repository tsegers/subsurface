// SPDX-License-Identifier: GPL-2.0
#ifndef DIVEPIXMAPCACHE_H
#define DIVEPIXMAPCACHE_H

#include <QPixmap>

// Since (some) pixmaps are rendered from SVG, which may be very slow,
// cache them once per ProfileScene. Different ProfileScenes may have
// different pixmap sizes. Scenes are created rarely (once per print
// job or UI window), so it should be fine to render the pixmaps
// on construction.
class DivePixmapCache {
public:
	DivePixmapCache(double dpr);
	QPixmap warning;
	QPixmap info;
	QPixmap violation;
	QPixmap bailout;
	QPixmap onCCRLoop;
	QPixmap bookmark;
	QPixmap gaschangeTrimixICD;
	QPixmap gaschangeTrimix;
	QPixmap gaschangeAirICD;
	QPixmap gaschangeAir;
	QPixmap gaschangeOxygenICD;
	QPixmap gaschangeOxygen;
	QPixmap gaschangeEANICD;
	QPixmap gaschangeEAN;
	QPixmap transparent;
};

#endif
