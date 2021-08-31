// SPDX-License-Identifier: GPL-2.0
#ifndef DIVEEVENTITEM_H
#define DIVEEVENTITEM_H

#include "divepixmapitem.h"

class DiveCartesianAxis;
class DivePixmapCache;
struct event;
struct plot_info;

class DiveEventItem : public DivePixmapItem {
	Q_OBJECT
public:
	DiveEventItem(const struct dive *d, struct event *ev, struct gasmix lastgasmix,
		      const struct plot_info &pi, DiveCartesianAxis *hAxis, DiveCartesianAxis *vAxis,
		      int speed, const DivePixmapCache &pixmaps, QGraphicsItem *parent = nullptr);
	~DiveEventItem();
	const struct event *getEvent() const;
	struct event *getEventMutable();
	void eventVisibilityChanged(const QString &eventName, bool visible);
	void setVerticalAxis(DiveCartesianAxis *axis, int speed);
	void setHorizontalAxis(DiveCartesianAxis *axis);
	bool shouldBeHidden();
	static bool isInteresting(const struct dive *d, const struct divecomputer *dc,
				  const struct event *ev, const struct plot_info &pi);
public
slots:
	void recalculatePos(int animationSpeed);

private:
	void setupToolTipString(struct gasmix lastgasmix);
	void setupPixmap(struct gasmix lastgasmix, const DivePixmapCache &pixmaps);
	DiveCartesianAxis *vAxis;
	DiveCartesianAxis *hAxis;
	struct event *ev;
	const struct dive *dive;
	int depth;
};

#endif // DIVEEVENTITEM_H
