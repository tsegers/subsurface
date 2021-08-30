// SPDX-License-Identifier: GPL-2.0
#include "profile-widget/diveeventitem.h"
#include "qt-models/diveplotdatamodel.h"
#include "profile-widget/divecartesianaxis.h"
#include "profile-widget/animationfunctions.h"
#include "core/event.h"
#include "core/format.h"
#include "core/libdivecomputer.h"
#include "core/profile.h"
#include "core/gettextfromc.h"
#include "core/metrics.h"
#include "core/sample.h"
#include "core/subsurface-string.h"
#include <QDebug>

#define DEPTH_NOT_FOUND (-2342)

DiveEventItem::DiveEventItem(const struct dive *d, struct event *ev, struct gasmix lastgasmix,
			     DivePlotDataModel *model, DiveCartesianAxis *hAxis, DiveCartesianAxis *vAxis,
			     int speed, double dpr, QGraphicsItem *parent) : DivePixmapItem(parent),
	vAxis(vAxis),
	hAxis(hAxis),
	dataModel(model),
	ev(ev),
	dive(d)
{
	setFlag(ItemIgnoresTransformations);

	setupPixmap(lastgasmix, dpr);
	setupToolTipString(lastgasmix);
	recalculatePos(0);


	connect(vAxis, &DiveCartesianAxis::sizeChanged, this,
		[speed, this] { recalculatePos(speed); });
}

DiveEventItem::~DiveEventItem()
{
}

const struct event *DiveEventItem::getEvent() const
{
	return ev;
}

struct event *DiveEventItem::getEventMutable()
{
	return ev;
}

void DiveEventItem::setupPixmap(struct gasmix lastgasmix, double dpr)
{
	extern int verbose;
	const IconMetrics& metrics = defaultIconMetrics();
#ifndef SUBSURFACE_MOBILE
	int sz_bigger = metrics.sz_med + metrics.sz_small; // ex 40px
#else
#if defined(Q_OS_IOS)
	 // on iOS devices we need to adjust for Device Pixel Ratio
	int sz_bigger = metrics.sz_med  * metrics.dpr;
#else
	// SUBSURFACE_MOBILE, seems a little big from the code,
	// but looks fine on device
	int sz_bigger = metrics.sz_big + metrics.sz_med;
#endif
#endif
	sz_bigger = lrint(sz_bigger * dpr);
	int sz_pix = sz_bigger/2; // ex 20px
	if (verbose)
		qDebug() << __FUNCTION__ << "DPR" << dpr << "metrics" << metrics.sz_med << metrics.sz_small << "sz_bigger" << sz_bigger;

#define EVENT_PIXMAP(PIX) QPixmap(QString(PIX)).scaled(sz_pix, sz_pix, Qt::KeepAspectRatio, Qt::SmoothTransformation)
#define EVENT_PIXMAP_BIGGER(PIX) QPixmap(QString(PIX)).scaled(sz_bigger, sz_bigger, Qt::KeepAspectRatio, Qt::SmoothTransformation)
	if (empty_string(ev->name)) {
		setPixmap(EVENT_PIXMAP(":status-warning-icon"));
	} else if (same_string_caseinsensitive(ev->name, "modechange")) {
		if (ev->value == 0)
			setPixmap(EVENT_PIXMAP(":bailout-icon"));
		else
			setPixmap(EVENT_PIXMAP(":onCCRLoop-icon"));
	} else if (ev->type == SAMPLE_EVENT_BOOKMARK) {
		setPixmap(EVENT_PIXMAP(":dive-bookmark-icon"));
	} else if (event_is_gaschange(ev)) {
		struct gasmix mix = get_gasmix_from_event(dive, ev);
		struct icd_data icd_data;
		bool icd = isobaric_counterdiffusion(lastgasmix, mix, &icd_data);
		if (mix.he.permille) {
			if (icd)
				setPixmap(EVENT_PIXMAP_BIGGER(":gaschange-trimix-ICD-icon"));
			else
				setPixmap(EVENT_PIXMAP_BIGGER(":gaschange-trimix-icon"));
		} else if (gasmix_is_air(mix)) {
			if (icd)
				setPixmap(EVENT_PIXMAP_BIGGER(":gaschange-air-ICD-icon"));
			else
				setPixmap(EVENT_PIXMAP_BIGGER(":gaschange-air-icon"));
		} else if (mix.o2.permille == 1000) {
			if (icd)
				setPixmap(EVENT_PIXMAP_BIGGER(":gaschange-oxygen-ICD-icon"));
			else
				setPixmap(EVENT_PIXMAP_BIGGER(":gaschange-oxygen-icon"));
		} else {
			if (icd)
				setPixmap(EVENT_PIXMAP_BIGGER(":gaschange-ean-ICD-icon"));
			else
				setPixmap(EVENT_PIXMAP_BIGGER(":gaschange-ean-icon"));
		}
#ifdef SAMPLE_FLAGS_SEVERITY_SHIFT
	} else if ((((ev->flags & SAMPLE_FLAGS_SEVERITY_MASK) >> SAMPLE_FLAGS_SEVERITY_SHIFT) == 1) ||
		    // those are useless internals of the dive computer
#else
	} else if (
#endif
		   same_string_caseinsensitive(ev->name, "heading") ||
		   (same_string_caseinsensitive(ev->name, "SP change") && ev->time.seconds == 0)) {
		// 2 cases:
		// a) some dive computers have heading in every sample
		// b) at t=0 we might have an "SP change" to indicate dive type
		// in both cases we want to get the right data into the tooltip but don't want the visual clutter
		// so set an "almost invisible" pixmap (a narrow but somewhat tall, basically transparent pixmap)
		// that allows tooltips to work when we don't want to show a specific
		// pixmap for an event, but want to show the event value in the tooltip
		QPixmap transparentPixmap(4, 20);
		transparentPixmap.fill(QColor::fromRgbF(1.0, 1.0, 1.0, 0.01));
		setPixmap(transparentPixmap);
#ifdef SAMPLE_FLAGS_SEVERITY_SHIFT
	} else if (((ev->flags & SAMPLE_FLAGS_SEVERITY_MASK) >> SAMPLE_FLAGS_SEVERITY_SHIFT) == 2) {
		setPixmap(EVENT_PIXMAP(":status-info-icon"));
	} else if (((ev->flags & SAMPLE_FLAGS_SEVERITY_MASK) >> SAMPLE_FLAGS_SEVERITY_SHIFT) == 3) {
		setPixmap(EVENT_PIXMAP(":status-warning-icon"));
	} else if (((ev->flags & SAMPLE_FLAGS_SEVERITY_MASK) >> SAMPLE_FLAGS_SEVERITY_SHIFT) == 4) {
		setPixmap(EVENT_PIXMAP(":status-violation-icon"));
#endif
	} else if (same_string_caseinsensitive(ev->name, "violation") || // generic libdivecomputer
		   same_string_caseinsensitive(ev->name, "Safety stop violation")  || // the rest are from the Uemis downloader
		   same_string_caseinsensitive(ev->name, "pO₂ ascend alarm")  ||
		   same_string_caseinsensitive(ev->name, "RGT alert")  ||
		   same_string_caseinsensitive(ev->name, "Dive time alert")  ||
		   same_string_caseinsensitive(ev->name, "Low battery alert")  ||
		   same_string_caseinsensitive(ev->name, "Speed alarm")) {
		setPixmap(EVENT_PIXMAP(":status-violation-icon"));
	} else if (same_string_caseinsensitive(ev->name, "non stop time") || // generic libdivecomputer
		   same_string_caseinsensitive(ev->name, "safety stop") ||
		   same_string_caseinsensitive(ev->name, "safety stop (voluntary)") ||
		   same_string_caseinsensitive(ev->name, "Tank change suggested") || // Uemis downloader
		   same_string_caseinsensitive(ev->name, "Marker")) {
		setPixmap(EVENT_PIXMAP(":status-info-icon"));
	} else {
		// we should do some guessing based on the type / name of the event;
		// for now they all get the warning icon
		setPixmap(EVENT_PIXMAP(":status-warning-icon"));
	}
#undef EVENT_PIXMAP
#undef EVENT_PIXMAP_BIGGER
}

void DiveEventItem::setupToolTipString(struct gasmix lastgasmix)
{
	// we display the event on screen - so translate
	QString name = gettextFromC::tr(ev->name);
	int value = ev->value;
	int type = ev->type;

	if (event_is_gaschange(ev)) {
		struct icd_data icd_data;
		struct gasmix mix = get_gasmix_from_event(dive, ev);
		name += ": ";
		name += gasname(mix);

		/* Do we have an explicit cylinder index?  Show it. */
		if (ev->gas.index >= 0)
			name += tr(" (cyl. %1)").arg(ev->gas.index + 1);
		bool icd = isobaric_counterdiffusion(lastgasmix, mix, &icd_data);
		if (icd_data.dHe < 0) {
			name += qasprintf_loc("\n%s %s:%+.3g%% %s:%+.3g%%%s%+.3g%%",
					      qPrintable(tr("ICD")),
					      qPrintable(tr("ΔHe")), icd_data.dHe / 10.0,
					      qPrintable(tr("ΔN₂")), icd_data.dN2 / 10.0,
					      icd ? ">" : "<", lrint(-icd_data.dHe / 5.0) / 10.0);
		}
	} else if (same_string(ev->name, "modechange")) {
		name += QString(": %1").arg(gettextFromC::tr(divemode_text_ui[ev->value]));
	} else if (value) {
		if (type == SAMPLE_EVENT_PO2 && same_string(ev->name, "SP change")) {
			name += QString(": %1bar").arg((double)value / 1000, 0, 'f', 1);
		} else if (type == SAMPLE_EVENT_CEILING && same_string(ev->name, "planned waypoint above ceiling")) {
			const char *depth_unit;
			double depth_value = get_depth_units(value*1000, NULL, &depth_unit);
			name += QString(": %1%2").arg((int) round(depth_value)).arg(depth_unit);
		} else {
			name += QString(": %1").arg(value);
		}
	} else if (type == SAMPLE_EVENT_PO2 && same_string(ev->name, "SP change")) {
		// this is a bad idea - we are abusing an existing event type that is supposed to
		// warn of high or low pO₂ and are turning it into a setpoint change event
		name += ":\n" + tr("Manual switch to OC");
	} else {
		name += ev->flags & SAMPLE_FLAGS_BEGIN ? tr(" begin", "Starts with space!") :
								    ev->flags & SAMPLE_FLAGS_END ? tr(" end", "Starts with space!") : "";
	}
	setToolTip(name);
}

void DiveEventItem::eventVisibilityChanged(const QString&, bool)
{
	//WARN: lookslike we should implement this.
}

bool DiveEventItem::shouldBeHidden()
{
	const struct divecomputer *dc = get_dive_dc_const(dive, dc_number);

	/*
	 * Some gas change events are special. Some dive computers just tell us the initial gas this way.
	 * Don't bother showing those
	 */
	const struct sample *first_sample = &dc->sample[0];
	if (!strcmp(ev->name, "gaschange") &&
	    (ev->time.seconds == 0 ||
	     (first_sample && ev->time.seconds == first_sample->time.seconds) ||
	     depthAtTime(ev->time.seconds) < SURFACE_THRESHOLD))
		return true;

	/*
	 * Some divecomputers give "surface" events that just aren't interesting.
	 * Like at the beginning or very end of a dive. Well, duh.
	 */
	if (!strcmp(ev->name, "surface")) {
		int time = ev->time.seconds;
		if (time <= 30 || time + 30 >= (int)dc->duration.seconds)
			return true;
	}

	for (int i = 0; i < evn_used; i++) {
		if (!strcmp(ev->name, ev_namelist[i].ev_name) && ev_namelist[i].plot_ev == false)
			return true;
	}
	return false;
}

int DiveEventItem::depthAtTime(int time)
{
	QModelIndexList result = dataModel->match(dataModel->index(0, DivePlotDataModel::TIME), Qt::DisplayRole, time);
	if (result.isEmpty()) {
		qWarning("can't find a spot in the dataModel");
		hide();
		return DEPTH_NOT_FOUND;
	}
	return dataModel->data(dataModel->index(result.first().row(), DivePlotDataModel::DEPTH)).toInt();
}

void DiveEventItem::recalculatePos(int speed)
{
	if (!ev)
		return;

	QModelIndexList result = dataModel->match(dataModel->index(0, DivePlotDataModel::TIME), Qt::DisplayRole, ev->time.seconds);
	if (result.isEmpty()) {
		qWarning("can't find a spot in the dataModel");
		hide();
		return;
	}
	int depth = depthAtTime(ev->time.seconds);
	if (depth == DEPTH_NOT_FOUND)
		return;
	if (!isVisible() && !shouldBeHidden())
		show();
	qreal x = hAxis->posAtValue(ev->time.seconds);
	qreal y = vAxis->posAtValue(depth);
	if (speed > 0)
		Animations::moveTo(this, speed, x, y);
	else
		setPos(x, y);
	if (isVisible() && shouldBeHidden())
		hide();
}
