// SPDX-License-Identifier: GPL-2.0
#include "profile-widget/profilewidget2.h"
#include "profile-widget/profilescene.h"
#include "qt-models/diveplotdatamodel.h"
#include "core/device.h"
#include "core/event.h"
#include "core/subsurface-string.h"
#include "core/qthelper.h"
#include "core/settings/qPrefTechnicalDetails.h"
#include "core/settings/qPrefPartialPressureGas.h"
#include "profile-widget/diveeventitem.h"
#include "profile-widget/divetextitem.h"
#include "profile-widget/divetooltipitem.h"
#include "profile-widget/divehandler.h"
#include "core/planner.h"
#include "profile-widget/ruleritem.h"
#include "core/pref.h"
#include "qt-models/diveplannermodel.h"
#include "qt-models/models.h"
#include "core/errorhelper.h"
#ifndef SUBSURFACE_MOBILE
#include "desktop-widgets/simplewidgets.h"
#include "commands/command.h"
#include "core/gettextfromc.h"
#include "core/imagedownloader.h"
#endif

#include <QScrollBar>
#include <QtCore/qmath.h>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <QWheelEvent>
#include <QMenu>
#include <QMimeData>
#include <QElapsedTimer>

#ifndef QT_NO_DEBUG
#include <QTableView>
#endif

// Constant describing at which z-level the thumbnails are located.
// We might add more constants here for easier customability.
#ifndef SUBSURFACE_MOBILE
static const double thumbnailBaseZValue = 100.0;
#endif

ProfileWidget2::ProfileWidget2(DivePlannerPointsModel *plannerModelIn, double fontPrintScale, QWidget *parent) : QGraphicsView(parent),
	profileScene(new ProfileScene(fontPrintScale, false, false)),
	currentState(INIT),
	plannerModel(plannerModelIn),
	zoomLevel(0),
	zoomFactor(1.15),
#ifndef SUBSURFACE_MOBILE
	toolTipItem(new ToolTipItem()),
#endif
	d(nullptr),
	dc(0),
#ifndef SUBSURFACE_MOBILE
	mouseFollowerVertical(new DiveLineItem()),
	mouseFollowerHorizontal(new DiveLineItem()),
	rulerItem(new RulerItem2()),
#endif
	shouldCalculateMax(true)
{
	setupSceneAndFlags();
	setupItemOnScene();
	addItemsToScene();
	scene()->installEventFilter(this);
#ifndef SUBSURFACE_MOBILE
	setAcceptDrops(true);

	addActionShortcut(Qt::Key_Escape, &ProfileWidget2::keyEscAction);
	addActionShortcut(Qt::Key_Delete, &ProfileWidget2::keyDeleteAction);
	addActionShortcut(Qt::Key_Up, &ProfileWidget2::keyUpAction);
	addActionShortcut(Qt::Key_Down, &ProfileWidget2::keyDownAction);
	addActionShortcut(Qt::Key_Left, &ProfileWidget2::keyLeftAction);
	addActionShortcut(Qt::Key_Right, &ProfileWidget2::keyRightAction);

	connect(Thumbnailer::instance(), &Thumbnailer::thumbnailChanged, this, &ProfileWidget2::updateThumbnail, Qt::QueuedConnection);
	connect(&diveListNotifier, &DiveListNotifier::picturesRemoved, this, &ProfileWidget2::picturesRemoved);
	connect(&diveListNotifier, &DiveListNotifier::picturesAdded, this, &ProfileWidget2::picturesAdded);
	connect(&diveListNotifier, &DiveListNotifier::cylinderEdited, this, &ProfileWidget2::profileChanged);
	connect(&diveListNotifier, &DiveListNotifier::eventsChanged, this, &ProfileWidget2::profileChanged);
	connect(&diveListNotifier, &DiveListNotifier::pictureOffsetChanged, this, &ProfileWidget2::pictureOffsetChanged);
	connect(&diveListNotifier, &DiveListNotifier::divesChanged, this, &ProfileWidget2::divesChanged);
	connect(&diveListNotifier, &DiveListNotifier::deviceEdited, this, &ProfileWidget2::replot);
#endif // SUBSURFACE_MOBILE

#if !defined(QT_NO_DEBUG) && defined(SHOW_PLOT_INFO_TABLE)
	QTableView *diveDepthTableView = new QTableView();
	diveDepthTableView->setModel(profileScene->dataModel);
	diveDepthTableView->show();
#endif

	auto tec = qPrefTechnicalDetails::instance();
	connect(tec, &qPrefTechnicalDetails::calcalltissuesChanged           , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::calcceilingChanged              , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::gflowChanged                    , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::gfhighChanged                   , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::dcceilingChanged                , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::eadChanged                      , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::calcceiling3mChanged            , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::modChanged                      , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::calcndlttsChanged               , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::hrgraphChanged                  , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::rulergraphChanged               , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::show_sacChanged                 , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::zoomed_plotChanged              , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::show_pictures_in_profileChanged , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::tankbarChanged                  , this, &ProfileWidget2::actionRequestedReplot);
	connect(tec, &qPrefTechnicalDetails::percentagegraphChanged          , this, &ProfileWidget2::actionRequestedReplot);

	auto pp_gas = qPrefPartialPressureGas::instance();
	connect(pp_gas, &qPrefPartialPressureGas::pheChanged, this, &ProfileWidget2::actionRequestedReplot);
	connect(pp_gas, &qPrefPartialPressureGas::pn2Changed, this, &ProfileWidget2::actionRequestedReplot);
	connect(pp_gas, &qPrefPartialPressureGas::po2Changed, this, &ProfileWidget2::actionRequestedReplot);

	setProfileState();
}

ProfileWidget2::~ProfileWidget2()
{
}

#ifndef SUBSURFACE_MOBILE
void ProfileWidget2::addActionShortcut(const Qt::Key shortcut, void (ProfileWidget2::*slot)())
{
	QAction *action = new QAction(this);
	action->setShortcut(shortcut);
	action->setShortcutContext(Qt::WindowShortcut);
	addAction(action);
	connect(action, &QAction::triggered, this, slot);
	actionsForKeys[shortcut] = action;
}
#endif // SUBSURFACE_MOBILE

void ProfileWidget2::addItemsToScene()
{
#ifndef SUBSURFACE_MOBILE
	scene()->addItem(toolTipItem);
	scene()->addItem(rulerItem);
	scene()->addItem(rulerItem->sourceNode());
	scene()->addItem(rulerItem->destNode());
	scene()->addItem(mouseFollowerHorizontal);
	scene()->addItem(mouseFollowerVertical);
	QPen pen(QColor(Qt::red).lighter());
	pen.setWidth(0);
	mouseFollowerHorizontal->setPen(pen);
	mouseFollowerVertical->setPen(pen);
#endif
}

void ProfileWidget2::setupItemOnScene()
{
#ifndef SUBSURFACE_MOBILE
	toolTipItem->setZValue(9998);
	toolTipItem->setTimeAxis(profileScene->timeAxis);
	rulerItem->setZValue(9997);
	rulerItem->setAxis(profileScene->timeAxis, profileScene->profileYAxis);
#endif
}

void ProfileWidget2::replot()
{
	plotDive(d, dc, false);
}

void ProfileWidget2::setupSceneAndFlags()
{
	setScene(profileScene.get());
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setOptimizationFlags(QGraphicsView::DontSavePainterState);
	setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
	setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
	setMouseTracking(true);
}

void ProfileWidget2::resetZoom()
{
	if (!zoomLevel)
		return;
	const qreal defScale = 1.0 / qPow(zoomFactor, (qreal)zoomLevel);
	scale(defScale, defScale);
	zoomLevel = 0;
}

// Currently just one dive, but the plan is to enable All of the selected dives.
void ProfileWidget2::plotDive(const struct dive *dIn, int dcIn, bool doClearPictures, bool instant)
{
	// If there was no previously displayed dive, turn off animations
	if (!d)
		instant = true;

	d = dIn;
	dc = dcIn;
	if (!d) {
		clear();
		return;
	}

	QElapsedTimer measureDuration; // let's measure how long this takes us (maybe we'll turn of TTL calculation later
	measureDuration.start();
#ifdef SUBSURFACE_MOBILE
	Q_UNUSED(doClearPictures);
#endif

	// restore default zoom level
	resetZoom();

	DivePlannerPointsModel *model = currentState == EDIT || currentState == PLAN ? plannerModel : nullptr;
	bool inPlanner = currentState == PLAN;

	profileScene->plotDive(d, dc, model, inPlanner, instant, shouldCalculateMax);

#ifndef SUBSURFACE_MOBILE
	// reset some item visibility on printMode changes
	toolTipItem->setVisible(!profileScene->printMode);
	rulerItem->setVisible(prefs.rulergraph && !profileScene->printMode && currentState != PLAN && currentState != EDIT);

	toolTipItem->setPlotInfo(profileScene->plotInfo);
	rulerItem->setPlotInfo(d, profileScene->plotInfo);

	if ((currentState == EDIT || currentState == PLAN) && plannerModel) {
		repositionDiveHandlers();
		plannerModel->deleteTemporaryPlan();
	}
	if (doClearPictures)
		clearPictures();
	else
		plotPicturesInternal(d, instant);

	toolTipItem->refresh(d, mapToScene(mapFromGlobal(QCursor::pos())), currentState == PLAN);
#endif

	// OK, how long did this take us? Anything above the second is way too long,
	// so if we are calculation TTS / NDL then let's force that off.
	qint64 elapsedTime = measureDuration.elapsed();
	if (verbose)
		qDebug() << "Profile calculation for dive " << d->number << "took" << elapsedTime << "ms" << " -- calculated ceiling preference is" << prefs.calcceiling;
	if (elapsedTime > 1000 && prefs.calcndltts) {
		qPrefTechnicalDetails::set_calcndltts(false);
		report_error(qPrintable(tr("Show NDL / TTS was disabled because of excessive processing time")));
	}
}

void ProfileWidget2::divesChanged(const QVector<dive *> &dives, DiveField field)
{
	// If the mode of the currently displayed dive changed, replot
	if (field.mode &&
	    std::any_of(dives.begin(), dives.end(),
			[id = displayed_dive.id] (const dive *d)
			{ return d->id == id; } ))
		replot();
}

void ProfileWidget2::actionRequestedReplot(bool)
{
	settingsChanged();
}

void ProfileWidget2::settingsChanged()
{
	profileScene->updateAxes(false);
	replot();
}

void ProfileWidget2::resizeEvent(QResizeEvent *event)
{
	QGraphicsView::resizeEvent(event);
	fitInView(sceneRect(), Qt::IgnoreAspectRatio);
}

#ifndef SUBSURFACE_MOBILE
void ProfileWidget2::mousePressEvent(QMouseEvent *event)
{
	if (zoomLevel)
		return;
	QGraphicsView::mousePressEvent(event);
	if (currentState == PLAN || currentState == EDIT)
		shouldCalculateMax = false;
}

void ProfileWidget2::divePlannerHandlerClicked()
{
	if (zoomLevel)
		return;
	shouldCalculateMax = false;
}

void ProfileWidget2::divePlannerHandlerReleased()
{
	if (zoomLevel)
		return;
	shouldCalculateMax = true;
	replot();
}

void ProfileWidget2::mouseReleaseEvent(QMouseEvent *event)
{
	if (zoomLevel)
		return;
	QGraphicsView::mouseReleaseEvent(event);
	if (currentState == PLAN || currentState == EDIT) {
		shouldCalculateMax = true;
		replot();
	}
}
#endif

void ProfileWidget2::scale(qreal sx, qreal sy)
{
	QGraphicsView::scale(sx, sy);

#ifndef SUBSURFACE_MOBILE
	// Since the zoom level changed, adjust the duration bars accordingly.
	// We want to grow/shrink the length, but not the height and pen.
	for (PictureEntry &p: pictures)
		updateDurationLine(p);

	// Since we created new duration lines, we have to update the order in which the thumbnails is painted.
	updateThumbnailPaintOrder();
#endif
}

#ifndef SUBSURFACE_MOBILE
void ProfileWidget2::wheelEvent(QWheelEvent *event)
{
	if (!d)
		return;
	QPoint toolTipPos = mapFromScene(toolTipItem->pos());
	if (event->buttons() == Qt::LeftButton)
		return;
	if (event->angleDelta().y() > 0 && zoomLevel < 20) {
		scale(zoomFactor, zoomFactor);
		zoomLevel++;
	} else if (event->angleDelta().y() < 0 && zoomLevel > 0) {
		// Zooming out
		scale(1.0 / zoomFactor, 1.0 / zoomFactor);
		zoomLevel--;
	}
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	scrollViewTo(event->position().toPoint());
#else
	scrollViewTo(event->pos());
#endif
	toolTipItem->setPos(mapToScene(toolTipPos));
}

void ProfileWidget2::mouseDoubleClickEvent(QMouseEvent *event)
{
	if ((currentState == PLAN || currentState == EDIT) && plannerModel) {
		QPointF mappedPos = mapToScene(event->pos());
		if (profileScene->isPointOutOfBoundaries(mappedPos))
			return;

		int minutes = lrint(profileScene->timeAxis->valueAt(mappedPos) / 60);
		int milimeters = lrint(profileScene->profileYAxis->valueAt(mappedPos) / M_OR_FT(1, 1)) * M_OR_FT(1, 1);
		plannerModel->addStop(milimeters, minutes * 60);
	}
}

void ProfileWidget2::scrollViewTo(const QPoint &pos)
{
	/* since we cannot use translate() directly on the scene we hack on
	 * the scroll bars (hidden) functionality */
	if (!zoomLevel || !d)
		return;
	QScrollBar *vs = verticalScrollBar();
	QScrollBar *hs = horizontalScrollBar();
	const qreal yRat = (qreal)pos.y() / viewport()->height();
	const qreal xRat = (qreal)pos.x() / viewport()->width();
	vs->setValue(lrint(yRat * vs->maximum()));
	hs->setValue(lrint(xRat * hs->maximum()));
}

void ProfileWidget2::mouseMoveEvent(QMouseEvent *event)
{
	QPointF pos = mapToScene(event->pos());
	toolTipItem->refresh(d, mapToScene(mapFromGlobal(QCursor::pos())), currentState == PLAN);

	if (zoomLevel == 0) {
		QGraphicsView::mouseMoveEvent(event);
	} else {
		QPoint toolTipPos = mapFromScene(toolTipItem->pos());
		scrollViewTo(event->pos());
		toolTipItem->setPos(mapToScene(toolTipPos));
	}

	qreal vValue = profileScene->profileYAxis->valueAt(pos);
	qreal hValue = profileScene->timeAxis->valueAt(pos);

	if (profileScene->profileYAxis->maximum() >= vValue && profileScene->profileYAxis->minimum() <= vValue) {
		mouseFollowerHorizontal->setPos(profileScene->timeAxis->pos().x(), pos.y());
	}
	if (profileScene->timeAxis->maximum() >= hValue && profileScene->timeAxis->minimum() <= hValue) {
		mouseFollowerVertical->setPos(pos.x(), profileScene->profileYAxis->line().y1());
	}
}

bool ProfileWidget2::eventFilter(QObject *object, QEvent *event)
{
	QGraphicsScene *s = qobject_cast<QGraphicsScene *>(object);
	if (s && event->type() == QEvent::GraphicsSceneHelp) {
		event->ignore();
		return true;
	}
	return QGraphicsView::eventFilter(object, event);
}
#endif

template <typename T>
static void hideAll(const T &container)
{
	for (auto &item: container)
		item->setVisible(false);
}

void ProfileWidget2::clear()
{
#ifndef SUBSURFACE_MOBILE
	clearPictures();
#endif
	disconnectTemporaryConnections();
	profileScene->clear();
	handles.clear();
	gases.clear();
}

void ProfileWidget2::setProfileState(const dive *dIn, int dcIn)
{
	d = dIn;
	dc = dcIn;

	setProfileState();
}

void ProfileWidget2::setProfileState()
{
	if (currentState == PROFILE)
		return;

	disconnectTemporaryConnections();

	currentState = PROFILE;
	setBackgroundBrush(getColor(::BACKGROUND, profileScene->isGrayscale));

	profileScene->updateAxes(true);

#ifndef SUBSURFACE_MOBILE
	toolTipItem->readPos();
	toolTipItem->setVisible(true);
	rulerItem->setVisible(prefs.rulergraph);
	mouseFollowerHorizontal->setVisible(false);
	mouseFollowerVertical->setVisible(false);
#endif

	handles.clear();
	gases.clear();
}

#ifndef SUBSURFACE_MOBILE
void ProfileWidget2::connectPlannerModel()
{
	connect(plannerModel, &DivePlannerPointsModel::dataChanged, this, &ProfileWidget2::replot);
	connect(plannerModel, &DivePlannerPointsModel::cylinderModelEdited, this, &ProfileWidget2::replot);
	connect(plannerModel, &DivePlannerPointsModel::modelReset, this, &ProfileWidget2::pointsReset);
	connect(plannerModel, &DivePlannerPointsModel::rowsInserted, this, &ProfileWidget2::pointInserted);
	connect(plannerModel, &DivePlannerPointsModel::rowsRemoved, this, &ProfileWidget2::pointsRemoved);
	connect(plannerModel, &DivePlannerPointsModel::rowsMoved, this, &ProfileWidget2::pointsMoved);
}

void ProfileWidget2::setEditState(const dive *d, int dc)
{
	if (currentState == EDIT)
		return;

	setProfileState(d, dc);
	mouseFollowerHorizontal->setVisible(true);
	mouseFollowerVertical->setVisible(true);
	mouseFollowerHorizontal->setLine(profileScene->timeAxis->line());
	mouseFollowerVertical->setLine(QLineF(0, profileScene->profileYAxis->pos().y(), 0, profileScene->timeAxis->pos().y()));
	disconnectTemporaryConnections();
	actionsForKeys[Qt::Key_Left]->setShortcut(Qt::Key_Left);
	actionsForKeys[Qt::Key_Right]->setShortcut(Qt::Key_Right);
	actionsForKeys[Qt::Key_Up]->setShortcut(Qt::Key_Up);
	actionsForKeys[Qt::Key_Down]->setShortcut(Qt::Key_Down);
	actionsForKeys[Qt::Key_Escape]->setShortcut(Qt::Key_Escape);
	actionsForKeys[Qt::Key_Delete]->setShortcut(Qt::Key_Delete);

	connectPlannerModel();

	/* show the same stuff that the profile shows. */
	currentState = EDIT; /* enable the add state. */
	setBackgroundBrush(QColor("#A7DCFF"));

	pointsReset();
	repositionDiveHandlers();
}

void ProfileWidget2::setPlanState(const dive *d, int dc)
{
	if (currentState == PLAN)
		return;

	setProfileState(d, dc);
	mouseFollowerHorizontal->setVisible(true);
	mouseFollowerVertical->setVisible(true);
	mouseFollowerHorizontal->setLine(profileScene->timeAxis->line());
	mouseFollowerVertical->setLine(QLineF(0, profileScene->profileYAxis->pos().y(), 0, profileScene->timeAxis->pos().y()));
	disconnectTemporaryConnections();
	actionsForKeys[Qt::Key_Left]->setShortcut(Qt::Key_Left);
	actionsForKeys[Qt::Key_Right]->setShortcut(Qt::Key_Right);
	actionsForKeys[Qt::Key_Up]->setShortcut(Qt::Key_Up);
	actionsForKeys[Qt::Key_Down]->setShortcut(Qt::Key_Down);
	actionsForKeys[Qt::Key_Escape]->setShortcut(Qt::Key_Escape);
	actionsForKeys[Qt::Key_Delete]->setShortcut(Qt::Key_Delete);

	connectPlannerModel();

	/* show the same stuff that the profile shows. */
	currentState = PLAN; /* enable the add state. */
	setBackgroundBrush(QColor("#D7E3EF"));

	pointsReset();
	repositionDiveHandlers();
}
#endif

bool ProfileWidget2::isPlanner() const
{
	return currentState == PLAN;
}

#if 0 // TODO::: FINISH OR DISABLE
struct int ProfileWidget2::getEntryFromPos(QPointF pos)
{
	// find the time stamp corresponding to the mouse position
	int seconds = lrint(timeAxis->valueAt(pos));

	for (int i = 0; i < plotInfo.nr; i++) {
		if (plotInfo.entry[i].sec >= seconds)
			return i;
	}
	return plotInfo.nr - 1;
}
#endif

#ifndef SUBSURFACE_MOBILE
/// Prints cylinder information for display.
/// eg : "Cyl 1 (AL80 EAN32)"
static QString printCylinderDescription(int i, const cylinder_t *cylinder)
{
	QString label = gettextFromC::tr("Cyl") + QString(" %1").arg(i+1);
	if( cylinder != NULL ) {
		QString mix = get_gas_string(cylinder->gasmix);
		label += QString(" (%2 %3)").arg(cylinder->type.description).arg(mix);
	}
	return label;
}

static bool isDiveTextItem(const QGraphicsItem *item, const DiveTextItem *textItem)
{
	while (item) {
		if (item == textItem)
			return true;
		item = item->parentItem();
	}
	return false;
}

void ProfileWidget2::contextMenuEvent(QContextMenuEvent *event)
{
	if (currentState == EDIT || currentState == PLAN) {
		QGraphicsView::contextMenuEvent(event);
		return;
	}
	QMenu m;
	if (!d)
		return;
	// figure out if we are ontop of the dive computer name in the profile
	QGraphicsItem *sceneItem = itemAt(mapFromGlobal(event->globalPos()));
	if (isDiveTextItem(sceneItem, profileScene->diveComputerText)) {
		const struct divecomputer *currentdc = get_dive_dc_const(d, dc);
		if (!currentdc->deviceid && dc == 0 && number_of_computers(d) == 1)
			// nothing to do, can't rename, delete or reorder
			return;
		// create menu to show when right clicking on dive computer name
		if (dc > 0)
			m.addAction(tr("Make first dive computer"), this, &ProfileWidget2::makeFirstDC);
		if (number_of_computers(d) > 1) {
			m.addAction(tr("Delete this dive computer"), this, &ProfileWidget2::deleteCurrentDC);
			m.addAction(tr("Split this dive computer into own dive"), this, &ProfileWidget2::splitCurrentDC);
		}
		if (currentdc->deviceid)
			m.addAction(tr("Rename this dive computer"), this, &ProfileWidget2::renameCurrentDC);
		m.exec(event->globalPos());
		// don't show the regular profile context menu
		return;
	}

	// create the profile context menu
	QPointF scenePos = mapToScene(mapFromGlobal(event->globalPos()));
	qreal sec_val = profileScene->timeAxis->valueAt(scenePos);
	int seconds = (sec_val < 0.0) ? 0 : (int)sec_val;
	DiveEventItem *item = dynamic_cast<DiveEventItem *>(sceneItem);

	// Add or edit Gas Change
	if (d && item && event_is_gaschange(item->getEvent())) {
		int eventTime = item->getEvent()->time.seconds;
		QMenu *gasChange = m.addMenu(tr("Edit Gas Change"));
		for (int i = 0; i < d->cylinders.nr; i++) {
			const cylinder_t *cylinder = get_cylinder(d, i);
			QString label = printCylinderDescription(i, cylinder);
			gasChange->addAction(label, [this, i, eventTime] { changeGas(i, eventTime); });
		}
	} else if (d && d->cylinders.nr > 1) {
		// if we have more than one gas, offer to switch to another one
		QMenu *gasChange = m.addMenu(tr("Add gas change"));
		for (int i = 0; i < d->cylinders.nr; i++) {
			const cylinder_t *cylinder = get_cylinder(d, i);
			QString label = printCylinderDescription(i, cylinder);
			gasChange->addAction(label, [this, i, seconds] { changeGas(i, seconds); });
		}
	}
	m.addAction(tr("Add setpoint change"), [this, seconds]() { ProfileWidget2::addSetpointChange(seconds); });
	m.addAction(tr("Add bookmark"), [this, seconds]() { addBookmark(seconds); });
	m.addAction(tr("Split dive into two"), [this, seconds]() { splitDive(seconds); });
	const struct event *ev = NULL;
	enum divemode_t divemode = UNDEF_COMP_TYPE;

	get_current_divemode(get_dive_dc_const(d, dc), seconds, &ev, &divemode);
	QMenu *changeMode = m.addMenu(tr("Change divemode"));
	if (divemode != OC)
		changeMode->addAction(gettextFromC::tr(divemode_text_ui[OC]),
				      [this, seconds](){ addDivemodeSwitch(seconds, OC); });
	if (divemode != CCR)
		changeMode->addAction(gettextFromC::tr(divemode_text_ui[CCR]),
				      [this, seconds](){ addDivemodeSwitch(seconds, CCR); });
	if (divemode != PSCR)
		changeMode->addAction(gettextFromC::tr(divemode_text_ui[PSCR]),
				      [this, seconds](){ addDivemodeSwitch(seconds, PSCR); });

	if (same_string(get_dive_dc_const(d, dc)->model, "manually added dive") || !get_dive_dc_const(d, dc)->samples)
		m.addAction(tr("Edit the profile"), this, &ProfileWidget2::editCurrentDive);

	if (DiveEventItem *item = dynamic_cast<DiveEventItem *>(sceneItem)) {
		m.addAction(tr("Remove event"), [this,item] { removeEvent(item); });
		m.addAction(tr("Hide similar events"), [this, item] { hideEvents(item); });
		const struct event *dcEvent = item->getEvent();
		if (dcEvent->type == SAMPLE_EVENT_BOOKMARK)
			m.addAction(tr("Edit name"), [this, item] { editName(item); });
#if 0 // TODO::: FINISH OR DISABLE
		QPointF scenePos = mapToScene(event->pos());
		int idx = getEntryFromPos(scenePos);
		// this shows how to figure out if we should ask the user if they want adjust interpolated pressures
		// at either side of a gas change
		if (dcEvent->type == SAMPLE_EVENT_GASCHANGE || dcEvent->type == SAMPLE_EVENT_GASCHANGE2) {
			qDebug() << "figure out if there are interpolated pressures";
			int gasChangeIdx = idx;
			while (gasChangeIdx > 0) {
				--gasChangeIdx;
				if (plotInfo.entry[gasChangeIdx].sec <= dcEvent->time.seconds)
					break;
			}
			const struct plot_data &gasChangeEntry = plotInfo.entry[newGasIdx];
			qDebug() << "at gas change at" << gasChangeEntry->sec << ": sensor pressure" << get_plot_sensor_pressure(&plotInfo, newGasIdx)
				 << "interpolated" << ;get_plot_sensor_pressure(&plotInfo, newGasIdx);
			// now gasChangeEntry points at the gas change, that entry has the final pressure of
			// the old tank, the next entry has the starting pressure of the next tank
			if (gasChangeIdx < plotInfo.nr - 1) {
				int newGasIdx = gasChangeIdx + 1;
				const struct plot_data &newGasEntry = plotInfo.entry[newGasIdx];
				qDebug() << "after gas change at " << newGasEntry->sec << ": sensor pressure" << newGasEntry->pressure[0] << "interpolated" << newGasEntry->pressure[1];
				if (get_plot_sensor_pressure(&plotInfo, gasChangeIdx) == 0 || get_cylinder(d, gasChangeEntry->sensor[0])->sample_start.mbar == 0) {
					// if we have no sensorpressure or if we have no pressure from samples we can assume that
					// we only have interpolated pressure (the pressure in the entry may be stored in the sensor
					// pressure field if this is the first or last entry for this tank... see details in gaspressures.c
					pressure_t pressure;
					pressure.mbar = get_plot_interpolated_pressure(&plotInfo, gasChangeIdx) ? : get_plot_sensor_pressure(&plotInfo, gasChangeIdx);
					QAction *adjustOldPressure = m.addAction(tr("Adjust pressure of cyl. %1 (currently interpolated as %2)")
										 .arg(gasChangeEntry->sensor[0] + 1).arg(get_pressure_string(pressure)));
				}
				if (get_plot_sensor_pressure(&plotInfo, newGasIdx) == 0 || get_cylinder(d, newGasEntry->sensor[0])->sample_start.mbar == 0) {
					// we only have interpolated press -- see commend above
					pressure_t pressure;
					pressure.mbar = get_plot_interpolated_pressure(&plotInfo, newGasIdx) ? : get_plot_sensor_pressure(&plotInfo, newGasIdx);
					QAction *adjustOldPressure = m.addAction(tr("Adjust pressure of cyl. %1 (currently interpolated as %2)")
										 .arg(newGasEntry->sensor[0] + 1).arg(get_pressure_string(pressure)));
				}
			}
		}
#endif
	}
	bool some_hidden = false;
	for (int i = 0; i < evn_used; i++) {
		if (ev_namelist[i].plot_ev == false) {
			some_hidden = true;
			break;
		}
	}
	if (some_hidden)
		m.addAction(tr("Unhide all events"), this, &ProfileWidget2::unhideEvents);
	m.exec(event->globalPos());
}

void ProfileWidget2::deleteCurrentDC()
{
	if (d)
		Command::deleteDiveComputer(mutable_dive(), dc);
}

void ProfileWidget2::splitCurrentDC()
{
	if (d)
		Command::splitDiveComputer(mutable_dive(), dc);
}

void ProfileWidget2::makeFirstDC()
{
	if (d)
		Command::moveDiveComputerToFront(mutable_dive(), dc);
}

void ProfileWidget2::renameCurrentDC()
{
	bool ok;
	QString newName = QInputDialog::getText(this, tr("Edit nickname"),
						tr("Set new nickname for %1 (serial %2):").arg(current_dc->model).arg(current_dc->serial),
						QLineEdit::Normal, get_dc_nickname(current_dc), &ok);
	if (ok)
		Command::editDeviceNickname(current_dc, newName);
}

void ProfileWidget2::hideEvents(DiveEventItem *item)
{
	const struct event *event = item->getEvent();

	if (QMessageBox::question(this,
				  TITLE_OR_TEXT(tr("Hide events"), tr("Hide all %1 events?").arg(event->name)),
				  QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok) {
		if (!empty_string(event->name)) {
			for (int i = 0; i < evn_used; i++) {
				if (same_string(event->name, ev_namelist[i].ev_name)) {
					ev_namelist[i].plot_ev = false;
					break;
				}
			}
			Q_FOREACH (DiveEventItem *evItem, profileScene->eventItems) {
				if (same_string(evItem->getEvent()->name, event->name))
					evItem->hide();
			}
		} else {
			item->hide();
		}
	}
}

void ProfileWidget2::unhideEvents()
{
	for (int i = 0; i < evn_used; i++) {
		ev_namelist[i].plot_ev = true;
	}
	Q_FOREACH (DiveEventItem *item, profileScene->eventItems)
		item->show();
}

void ProfileWidget2::removeEvent(DiveEventItem *item)
{
	struct event *event = item->getEventMutable();
	if (!event || !d)
		return;

	if (QMessageBox::question(this, TITLE_OR_TEXT(
					  tr("Remove the selected event?"),
					  tr("%1 @ %2:%3").arg(event->name).arg(event->time.seconds / 60).arg(event->time.seconds % 60, 2, 10, QChar('0'))),
				  QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok)
		Command::removeEvent(mutable_dive(), dc, event);
}

void ProfileWidget2::addBookmark(int seconds)
{
	if (d)
		Command::addEventBookmark(mutable_dive(), dc, seconds);
}

void ProfileWidget2::addDivemodeSwitch(int seconds, int divemode)
{
	if (d)
		Command::addEventDivemodeSwitch(mutable_dive(), dc, seconds, divemode);
}

void ProfileWidget2::addSetpointChange(int seconds)
{
	if (!d)
		return;
	SetpointDialog dialog(mutable_dive(), dc, seconds);
	dialog.exec();
}

void ProfileWidget2::splitDive(int seconds)
{
	if (!d)
		return;
	Command::splitDives(mutable_dive(), duration_t{ seconds });
}

void ProfileWidget2::changeGas(int tank, int seconds)
{
	if (!d || tank < 0 || tank >= d->cylinders.nr)
		return;

	Command::addGasSwitch(mutable_dive(), dc, seconds, tank);
}
#endif

#ifndef SUBSURFACE_MOBILE
void ProfileWidget2::editName(DiveEventItem *item)
{
	struct event *event = item->getEventMutable();
	if (!event || !d)
		return;
	bool ok;
	QString newName = QInputDialog::getText(this, tr("Edit name of bookmark"),
						tr("Custom name:"), QLineEdit::Normal,
						event->name, &ok);
	if (ok && !newName.isEmpty()) {
		if (newName.length() > 22) { //longer names will display as garbage.
			QMessageBox lengthWarning;
			lengthWarning.setText(tr("Name is too long!"));
			lengthWarning.exec();
			return;
		}
		Command::renameEvent(mutable_dive(), dc, event, qPrintable(newName));
	}
}
#endif

void ProfileWidget2::disconnectTemporaryConnections()
{
#ifndef SUBSURFACE_MOBILE
	if (plannerModel) {
		disconnect(plannerModel, &DivePlannerPointsModel::dataChanged, this, &ProfileWidget2::replot);
		disconnect(plannerModel, &DivePlannerPointsModel::cylinderModelEdited, this, &ProfileWidget2::replot);

		disconnect(plannerModel, &DivePlannerPointsModel::modelReset, this, &ProfileWidget2::pointsReset);
		disconnect(plannerModel, &DivePlannerPointsModel::rowsInserted, this, &ProfileWidget2::pointInserted);
		disconnect(plannerModel, &DivePlannerPointsModel::rowsRemoved, this, &ProfileWidget2::pointsRemoved);
		disconnect(plannerModel, &DivePlannerPointsModel::rowsMoved, this, &ProfileWidget2::pointsMoved);
	}
#endif
	Q_FOREACH (QAction *action, actionsForKeys.values()) {
		action->setShortcut(QKeySequence());
		action->setShortcutContext(Qt::WidgetShortcut);
	}
}

int ProfileWidget2::handleIndex(const DiveHandler *h) const
{
	auto it = std::find_if(handles.begin(), handles.end(),
			       [h] (const std::unique_ptr<DiveHandler> &h2)
			       { return h == h2.get(); });
	return it != handles.end() ? it - handles.begin() : -1;
}

#ifndef SUBSURFACE_MOBILE

DiveHandler *ProfileWidget2::createHandle()
{
	DiveHandler *item = new DiveHandler(d);
	scene()->addItem(item);
	connect(item, &DiveHandler::moved, this, &ProfileWidget2::divePlannerHandlerMoved);
	connect(item, &DiveHandler::clicked, this, &ProfileWidget2::divePlannerHandlerClicked);
	connect(item, &DiveHandler::released, this, &ProfileWidget2::divePlannerHandlerReleased);
	return item;
}

QGraphicsSimpleTextItem *ProfileWidget2::createGas()
{
	QGraphicsSimpleTextItem *gasChooseBtn = new QGraphicsSimpleTextItem();
	scene()->addItem(gasChooseBtn);
	gasChooseBtn->setZValue(10);
	gasChooseBtn->setFlag(QGraphicsItem::ItemIgnoresTransformations);
	return gasChooseBtn;
}

void ProfileWidget2::pointsReset()
{
	handles.clear();
	gases.clear();
	int count = plannerModel->rowCount();
	for (int i = 0; i < count; ++i) {
		handles.emplace_back(createHandle());
		gases.emplace_back(createGas());
	}
}

void ProfileWidget2::pointInserted(const QModelIndex &, int from, int to)
{
	for (int i = from; i <= to; ++i) {
		handles.emplace(handles.begin() + i, createHandle());
		gases.emplace(gases.begin() + i, createGas());
	}

	// Note: we don't replot the dive here, because when removing multiple
	// points, these might trickle in one-by-one. Instead, the model will
	// emit a data-changed signal.
}

void ProfileWidget2::pointsRemoved(const QModelIndex &, int start, int end)
{
	// Qt's model/view API is mad. The end-point is inclusive, which means that the empty range is [0,-1]!
	handles.erase(handles.begin() + start, handles.begin() + end + 1);
	gases.erase(gases.begin() + start, gases.begin() + end + 1);
	scene()->clearSelection();

	// Note: we don't replot the dive here, because when removing multiple
	// points, these might trickle in one-by-one. Instead, the model will
	// emit a data-changed signal.
}

void ProfileWidget2::pointsMoved(const QModelIndex &, int start, int end, const QModelIndex &, int row)
{
	moveInVector(handles, start, end + 1, row);
	moveInVector(gases, start, end + 1, row);
}

void ProfileWidget2::repositionDiveHandlers()
{
	hideAll(gases);
	// Re-position the user generated dive handlers
	for (int i = 0; i < plannerModel->rowCount(); i++) {
		struct divedatapoint datapoint = plannerModel->at(i);
		if (datapoint.time == 0) // those are the magic entries for tanks
			continue;
		DiveHandler *h = handles[i].get();
		h->setVisible(datapoint.entered);
		h->setPos(profileScene->timeAxis->posAtValue(datapoint.time), profileScene->profileYAxis->posAtValue(datapoint.depth.mm));
		QPointF p1;
		if (i == 0) {
			if (prefs.drop_stone_mode)
				// place the text on the straight line from the drop to stone position
				p1 = QPointF(profileScene->timeAxis->posAtValue(datapoint.depth.mm / prefs.descrate),
					     profileScene->profileYAxis->posAtValue(datapoint.depth.mm));
			else
				// place the text on the straight line from the origin to the first position
				p1 = QPointF(profileScene->timeAxis->posAtValue(0), profileScene->profileYAxis->posAtValue(0));
		} else {
			// place the text on the line from the last position
			p1 = handles[i - 1]->pos();
		}
		QPointF p2 = handles[i]->pos();
		QLineF line(p1, p2);
		QPointF pos = line.pointAt(0.5);
		gases[i]->setPos(pos);
		if (datapoint.cylinderid >= 0 && datapoint.cylinderid < d->cylinders.nr)
			gases[i]->setText(get_gas_string(get_cylinder(d, datapoint.cylinderid)->gasmix));
		else
			gases[i]->setText(QString());
		gases[i]->setVisible(datapoint.entered &&
				(i == 0 || gases[i]->text() != gases[i-1]->text()));
	}
}

void ProfileWidget2::divePlannerHandlerMoved()
{
	DiveHandler *activeHandler = qobject_cast<DiveHandler *>(sender());
	int index = handleIndex(activeHandler);

	// Grow the time axis if necessary.
	int minutes = lrint(profileScene->timeAxis->valueAt(activeHandler->pos()) / 60);
	if (minutes * 60 > profileScene->timeAxis->maximum() * 0.9)
		profileScene->timeAxis->setMaximum(profileScene->timeAxis->maximum() * 1.02);

	divedatapoint data = plannerModel->at(index);
	data.depth.mm = lrint(profileScene->profileYAxis->valueAt(activeHandler->pos()) / M_OR_FT(1, 1)) * M_OR_FT(1, 1);
	data.time = lrint(profileScene->timeAxis->valueAt(activeHandler->pos()));

	plannerModel->editStop(index, data);
}

void ProfileWidget2::keyDownAction()
{
	if ((currentState != EDIT && currentState != PLAN) || !plannerModel)
		return;

	Q_FOREACH (QGraphicsItem *i, scene()->selectedItems()) {
		if (DiveHandler *handler = qgraphicsitem_cast<DiveHandler *>(i)) {
			int row = handleIndex(handler);
			divedatapoint dp = plannerModel->at(row);

			dp.depth.mm += M_OR_FT(1, 5);
			plannerModel->editStop(row, dp);
		}
	}
}

void ProfileWidget2::keyUpAction()
{
	if ((currentState != EDIT && currentState != PLAN) || !plannerModel)
		return;

	Q_FOREACH (QGraphicsItem *i, scene()->selectedItems()) {
		if (DiveHandler *handler = qgraphicsitem_cast<DiveHandler *>(i)) {
			int row = handleIndex(handler);
			divedatapoint dp = plannerModel->at(row);

			if (dp.depth.mm <= 0)
				continue;

			dp.depth.mm -= M_OR_FT(1, 5);
			plannerModel->editStop(row, dp);
		}
	}
}

void ProfileWidget2::keyLeftAction()
{
	if ((currentState != EDIT && currentState != PLAN) || !plannerModel)
		return;

	Q_FOREACH (QGraphicsItem *i, scene()->selectedItems()) {
		if (DiveHandler *handler = qgraphicsitem_cast<DiveHandler *>(i)) {
			int row = handleIndex(handler);
			divedatapoint dp = plannerModel->at(row);

			if (dp.time / 60 <= 0)
				continue;

			dp.time -= 60;
			plannerModel->editStop(row, dp);
		}
	}
}

void ProfileWidget2::keyRightAction()
{
	if ((currentState != EDIT && currentState != PLAN) || !plannerModel)
		return;

	Q_FOREACH (QGraphicsItem *i, scene()->selectedItems()) {
		if (DiveHandler *handler = qgraphicsitem_cast<DiveHandler *>(i)) {
			int row = handleIndex(handler);
			divedatapoint dp = plannerModel->at(row);

			dp.time += 60;
			plannerModel->editStop(row, dp);
		}
	}
}

void ProfileWidget2::keyDeleteAction()
{
	if ((currentState != EDIT && currentState != PLAN) || !plannerModel)
		return;

	QVector<int> selectedIndices;
	Q_FOREACH (QGraphicsItem *i, scene()->selectedItems()) {
		if (DiveHandler *handler = qgraphicsitem_cast<DiveHandler *>(i)) {
			selectedIndices.push_back(handleIndex(handler));
			handler->hide();
		}
	}
	if (!selectedIndices.isEmpty())
		plannerModel->removeSelectedPoints(selectedIndices);
}

void ProfileWidget2::keyEscAction()
{
	if ((currentState != EDIT && currentState != PLAN) || !plannerModel)
		return;

	if (scene()->selectedItems().count()) {
		scene()->clearSelection();
		return;
	}

	if (plannerModel->isPlanner())
		plannerModel->cancelPlan();
}

void ProfileWidget2::clearPictures()
{
	pictures.clear();
}

static const double unscaledDurationLineWidth = 2.5;
static const double unscaledDurationLinePenWidth = 0.5;

// Reset the duration line after an image was moved or we found a new duration
void ProfileWidget2::updateDurationLine(PictureEntry &e)
{
	if (e.duration.seconds > 0) {
		// We know the duration of this video, reset the line symbolizing its extent accordingly
		double begin = profileScene->timeAxis->posAtValue(e.offset.seconds);
		double end = profileScene->timeAxis->posAtValue(e.offset.seconds + e.duration.seconds);
		double y = e.thumbnail->y();

		// Undo scaling for pen-width and line-width. For this purpose, we use the scaling of the y-axis.
		double scale = transform().m22();
		double durationLineWidth = unscaledDurationLineWidth / scale;
		double durationLinePenWidth = unscaledDurationLinePenWidth / scale;
		e.durationLine.reset(new QGraphicsRectItem(begin, y - durationLineWidth - durationLinePenWidth, end - begin, durationLineWidth));
		e.durationLine->setPen(QPen(getColor(DURATION_LINE, profileScene->isGrayscale), durationLinePenWidth));
		e.durationLine->setBrush(getColor(::BACKGROUND, profileScene->isGrayscale));
		e.durationLine->setVisible(prefs.show_pictures_in_profile);
		scene()->addItem(e.durationLine.get());
	} else {
		// This is either a picture or a video with unknown duration.
		// In case there was a line (how could that be?) remove it.
		e.durationLine.reset();
	}
}

// This function is called asynchronously by the thumbnailer if a thumbnail
// was fetched from disk or freshly calculated.
void ProfileWidget2::updateThumbnail(QString filename, QImage thumbnail, duration_t duration)
{
	// Find the picture with the given filename
	auto it = std::find_if(pictures.begin(), pictures.end(), [&filename](const PictureEntry &e)
			       { return e.filename == filename; });

	// If we didn't find a picture, it does either not belong to the current dive,
	// or its timestamp is outside of the profile.
	if (it != pictures.end()) {
		// Replace the pixmap of the thumbnail with the newly calculated one.
		int size = Thumbnailer::defaultThumbnailSize();
		it->thumbnail->setPixmap(QPixmap::fromImage(thumbnail.scaled(size, size, Qt::KeepAspectRatio)));

		// If the duration changed, update the line
		if (duration.seconds != it->duration.seconds) {
			it->duration = duration;
			updateDurationLine(*it);
			// If we created / removed a duration line, we have to update the thumbnail paint order.
			updateThumbnailPaintOrder();
		}
	}
}

// Create a PictureEntry object and add its thumbnail to the scene if profile pictures are shown.
ProfileWidget2::PictureEntry::PictureEntry(offset_t offsetIn, const QString &filenameIn, ProfileWidget2 *profile, bool synchronous) : offset(offsetIn),
	duration(duration_t {0}),
	filename(filenameIn),
	thumbnail(new DivePictureItem)
{
	QGraphicsScene *scene = profile->scene();
	int size = Thumbnailer::defaultThumbnailSize();
	scene->addItem(thumbnail.get());
	thumbnail->setVisible(prefs.show_pictures_in_profile);
	QImage img = Thumbnailer::instance()->fetchThumbnail(filename, synchronous).scaled(size, size, Qt::KeepAspectRatio);
	thumbnail->setPixmap(QPixmap::fromImage(img));
	thumbnail->setFileUrl(filename);
	connect(thumbnail.get(), &DivePictureItem::removePicture, profile, &ProfileWidget2::removePicture);
}

// Define a default sort order for picture-entries: sort lexicographically by timestamp and filename.
bool ProfileWidget2::PictureEntry::operator< (const PictureEntry &e) const
{
	// Use std::tie() for lexicographical sorting.
	return std::tie(offset.seconds, filename) < std::tie(e.offset.seconds, e.filename);
}

// This function updates the paint order of the thumbnails and duration-lines, such that later
// thumbnails are painted on top of previous thumbnails and duration-lines on top of the thumbnail
// they belong to.
void ProfileWidget2::updateThumbnailPaintOrder()
{
	if (!pictures.size())
		return;
	// To get the correct sort order, we place in thumbnails at equal z-distances
	// between thumbnailBaseZValue and (thumbnailBaseZValue + 1.0).
	// Duration-lines are placed between the thumbnails.
	double z = thumbnailBaseZValue;
	double step = 1.0 / (double)pictures.size();
	for (PictureEntry &e: pictures) {
		e.thumbnail->setBaseZValue(z);
		if (e.durationLine)
			e.durationLine->setZValue(z + step / 2.0);
		z += step;
	}
}

// Calculate the y-coordinates of the thumbnails, which are supposed to be sorted by x-coordinate.
// This will also change the order in which the thumbnails are painted, to avoid weird effects,
// when items are added later to the scene. This is done using the QGraphicsItem::packBefore() function.
// We can't use the z-value, because that will be modified on hoverEnter and hoverExit events.
void ProfileWidget2::calculatePictureYPositions()
{
	double lastX = -1.0, lastY = 0.0;
	for (PictureEntry &e: pictures) {
		// let's put the picture at the correct time, but at a fixed "depth" on the profile
		// not sure this is ideal, but it seems to look right.
		double x = e.thumbnail->x();
		double y;
		if (lastX >= 0.0 && fabs(x - lastX) < 3 && lastY <= (10 + 14 * 3))
			y = lastY + 3;
		else
			y = 10;
		lastX = x;
		lastY = y;
		e.thumbnail->setY(y);
		updateDurationLine(e); // If we changed the y-position, we also have to change the duration-line.
	}
	updateThumbnailPaintOrder();
}

void ProfileWidget2::updateThumbnailXPos(PictureEntry &e)
{
	// Here, we only set the x-coordinate of the picture. The y-coordinate
	// will be set later in calculatePictureYPositions().
	double x = profileScene->timeAxis->posAtValue(e.offset.seconds);
	e.thumbnail->setX(x);
}

// This function resets the picture thumbnails of the current dive.
void ProfileWidget2::plotPictures()
{
	plotPicturesInternal(d, false);
}

void ProfileWidget2::plotPicturesInternal(const struct dive *d, bool synchronous)
{
	pictures.clear();
	if (currentState == EDIT || currentState == PLAN)
		return;

	// Fetch all pictures of the dive, but consider only those that are within the dive time.
	// For each picture, create a PictureEntry object in the pictures-vector.
	// emplace_back() constructs an object at the end of the vector. The parameters are passed directly to the constructor.
	// Note that FOR_EACH_PICTURE handles d being null gracefully.
	FOR_EACH_PICTURE(d) {
		if (picture->offset.seconds > 0 && picture->offset.seconds <= d->duration.seconds)
			pictures.emplace_back(picture->offset, QString(picture->filename), this, synchronous);
	}
	if (pictures.empty())
		return;
	// Sort pictures by timestamp (and filename if equal timestamps).
	// This will allow for proper location of the pictures on the profile plot.
	std::sort(pictures.begin(), pictures.end());

	// Calculate thumbnail positions. First the x-coordinates and and then the y-coordinates.
	for (PictureEntry &e: pictures)
		updateThumbnailXPos(e);
	calculatePictureYPositions();
}

// Remove the pictures with the given filenames from the profile plot.
void ProfileWidget2::picturesRemoved(dive *d, QVector<QString> fileUrls)
{
	// To remove the pictures, we use the std::remove_if() algorithm.
	// std::remove_if() does not actually delete the elements, but moves
	// them to the end of the given range. It returns an iterator to the
	// end of the new range of non-deleted elements. A subsequent call to
	// std::erase on the range of deleted elements then ultimately shrinks the vector.
	// (c.f. erase-remove idiom: https://en.wikipedia.org/wiki/Erase%E2%80%93remove_idiom)
	auto it = std::remove_if(pictures.begin(), pictures.end(), [&fileUrls](const PictureEntry &e)
			// Check whether filename of entry is in list of provided filenames
			{ return std::find(fileUrls.begin(), fileUrls.end(), e.filename) != fileUrls.end(); });
	pictures.erase(it, pictures.end());
	calculatePictureYPositions();
}

void ProfileWidget2::picturesAdded(dive *d, QVector<PictureObj> pics)
{
	for (const PictureObj &pic: pics) {
		if (pic.offset.seconds > 0 && pic.offset.seconds <= d->duration.seconds) {
			pictures.emplace_back(pic.offset, QString::fromStdString(pic.filename), this, false);
			updateThumbnailXPos(pictures.back());
		}
	}

	// Sort pictures by timestamp (and filename if equal timestamps).
	// This will allow for proper location of the pictures on the profile plot.
	std::sort(pictures.begin(), pictures.end());

	calculatePictureYPositions();
}

void ProfileWidget2::removePicture(const QString &fileUrl)
{
	if (d)
		Command::removePictures({ { mutable_dive(), { fileUrl.toStdString() } } });
}

void ProfileWidget2::profileChanged(dive *dive)
{
	if (dive != d)
		return; // Cylinders of a differnt dive than the shown one changed.
	replot();
}

#endif

void ProfileWidget2::dropEvent(QDropEvent *event)
{
#ifndef SUBSURFACE_MOBILE
	if (event->mimeData()->hasFormat("application/x-subsurfaceimagedrop") && d) {
		QByteArray itemData = event->mimeData()->data("application/x-subsurfaceimagedrop");
		QDataStream dataStream(&itemData, QIODevice::ReadOnly);

		QString filename;
		dataStream >> filename;
		QPointF mappedPos = mapToScene(event->pos());
		offset_t offset { (int32_t)lrint(profileScene->timeAxis->valueAt(mappedPos)) };
		Command::setPictureOffset(mutable_dive(), filename, offset);

		if (event->source() == this) {
			event->setDropAction(Qt::MoveAction);
			event->accept();
		} else {
			event->acceptProposedAction();
		}
	} else {
		event->ignore();
	}
#endif
}

#ifndef SUBSURFACE_MOBILE
void ProfileWidget2::pictureOffsetChanged(dive *dIn, QString filename, offset_t offset)
{
	if (dIn != d)
		return; // Picture of a different dive than the one shown changed.

	// Calculate time in dive where picture was dropped and whether the new position is during the dive.
	bool duringDive = d && offset.seconds > 0 && offset.seconds < d->duration.seconds;

	// A picture was drag&dropped onto the profile: We have four cases to consider:
	//	1a) The image was already shown on the profile and is moved to a different position on the profile.
	//	    Calculate the new position and move the picture.
	//	1b) The image was on the profile and is moved outside of the dive time.
	//	    Remove the picture.
	//	2a) The image was not on the profile and is moved into the dive time.
	//	    Add the picture to the profile.
	//	2b) The image was not on the profile and is moved outside of the dive time.
	//	    Do nothing.
	auto oldPos = std::find_if(pictures.begin(), pictures.end(), [filename](const PictureEntry &e)
				   { return e.filename == filename; });
	if (oldPos != pictures.end()) {
		// Cases 1a) and 1b): picture is on profile
		if (duringDive) {
			// Case 1a): move to new position
			// First, find new position. Note that we also have to compare filenames,
			// because it is quite easy to generate equal offsets.
			auto newPos = std::find_if(pictures.begin(), pictures.end(), [offset, &filename](const PictureEntry &e)
						   { return std::tie(e.offset.seconds, e.filename) > std::tie(offset.seconds, filename); });
			// Set new offset
			oldPos->offset.seconds = offset.seconds;
			updateThumbnailXPos(*oldPos);

			// Move image from old to new position
			int oldIndex = oldPos - pictures.begin();
			int newIndex = newPos - pictures.begin();
			moveInVector(pictures, oldIndex, oldIndex + 1, newIndex);
		} else {
			// Case 1b): remove picture
			pictures.erase(oldPos);
		}

		// In both cases the picture list changed, therefore we must recalculate the y-coordinatesA.
		calculatePictureYPositions();
	} else {
		// Cases 2a) and 2b): picture not on profile. We only have to take action for
		// the first case: picture is moved into dive-time.
		if (duringDive) {
			// Case 2a): add the picture at the appropriate position.
			// The case move from outside-to-outside of the profile plot was handled by
			// the "&& duringDive" condition in the if above.
			// As for case 1a), we have to also consider filenames in the case of equal offsets.
			auto newPos = std::find_if(pictures.begin(), pictures.end(), [offset, &filename](const PictureEntry &e)
						   { return std::tie(e.offset.seconds, e.filename) > std::tie(offset.seconds, filename); });
			// emplace() constructs the element at the given position in the vector.
			// The parameters are passed directly to the contructor.
			// The call returns an iterator to the new element (which might differ from
			// the old iterator, since the buffer might have been reallocated).
			newPos = pictures.emplace(newPos, offset, filename, this, false);
			updateThumbnailXPos(*newPos);
			calculatePictureYPositions();
		}
	}
}
#endif

void ProfileWidget2::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("application/x-subsurfaceimagedrop")) {
		if (event->source() == this) {
			event->setDropAction(Qt::MoveAction);
			event->accept();
		} else {
			event->acceptProposedAction();
		}
	} else {
		event->ignore();
	}
}

void ProfileWidget2::dragMoveEvent(QDragMoveEvent *event)
{
	if (event->mimeData()->hasFormat("application/x-subsurfaceimagedrop")) {
		if (event->source() == this) {
			event->setDropAction(Qt::MoveAction);
			event->accept();
		} else {
			event->acceptProposedAction();
		}
	} else {
		event->ignore();
	}
}

struct dive *ProfileWidget2::mutable_dive() const
{
	return const_cast<dive *>(d);
}
