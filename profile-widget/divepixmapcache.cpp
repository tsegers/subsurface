// SPDX-License-Identifier: GPL-2.0
#include "divepixmapcache.h"
#include "core/metrics.h"
#include "core/qthelper.h" // for renderSVGIconWidth

#include <cmath>

static QPixmap createPixmap(const char *name, int size)
{
	return QPixmap(QString(name)).scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

DivePixmapCache::DivePixmapCache(double dpr)
{
	extern int verbose;
	const IconMetrics &metrics = defaultIconMetrics();
#ifndef SUBSURFACE_MOBILE
	int sz_bigger = metrics.sz_med + metrics.sz_small; // ex 40px
#else
	// SUBSURFACE_MOBILE, seems a little big from the code,
	// but looks fine on device
	int sz_bigger = metrics.sz_big + metrics.sz_med;
#endif
	sz_bigger = lrint(sz_bigger * dpr);
	int sz_pix = sz_bigger / 2; // ex 20px
	if (verbose)
		qDebug("%s DPR: %f metrics: %d %d sz_bigger: %d", __FUNCTION__, dpr, metrics.sz_med, metrics.sz_small, sz_bigger);

	warning = createPixmap(":status-warning-icon", sz_pix);
	info = createPixmap(":status-info-icon", sz_pix);
	violation = createPixmap(":status-violation-icon", sz_pix);
	bailout = createPixmap(":bailout-icon", sz_pix);
	onCCRLoop = createPixmap(":onCCRLoop-icon", sz_pix);
	bookmark = QPixmap::fromImage(renderSVGIconWidth(":dive-bookmark-icon", sz_pix));
	gaschangeTrimixICD = createPixmap(":gaschange-trimix-ICD-icon", sz_bigger);
	gaschangeTrimix = createPixmap(":gaschange-trimix-icon", sz_bigger);
	gaschangeAirICD = createPixmap(":gaschange-air-ICD-icon", sz_bigger);
	gaschangeAir = createPixmap(":gaschange-air-icon", sz_bigger);
	gaschangeOxygenICD = createPixmap(":gaschange-oxygen-ICD-icon", sz_bigger);
	gaschangeOxygen = createPixmap(":gaschange-oxygen-icon", sz_bigger);
	gaschangeEANICD = createPixmap(":gaschange-ean-ICD-icon", sz_bigger);
	gaschangeEAN = createPixmap(":gaschange-ean-icon", sz_bigger);

	// The transparen pixmap is a very obscure feature to enable tooltips without showing a pixmap.
	// See code in diveeventitem.cpp. This should probably be replaced by a different mechanism.
	QPixmap transparentPixmap(lrint(4 * dpr), lrint(20 * dpr));
	transparentPixmap.fill(QColor::fromRgbF(1.0, 1.0, 1.0, 0.01));
}
