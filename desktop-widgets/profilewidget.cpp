// SPDX-License-Identifier: GPL-2.0

#include "profilewidget.h"
#include "profile-widget/profilewidget2.h"
#include "core/settings/qPrefTechnicalDetails.h"
#include "core/settings/qPrefPartialPressureGas.h"
#include "qt-models/diveplannermodel.h"

#include <QToolBar>
#include <QHBoxLayout>

ProfileWidget::ProfileWidget()
{
	ui.setupUi(this);

	// what is a sane order for those icons? we should have the ones the user is
	// most likely to want towards the top so they are always visible
	// and the ones that someone likely sets and then never touches again towards the bottom
	toolbarActions = { ui.profCalcCeiling, ui.profCalcAllTissues, // start with various ceilings
			   ui.profIncrement3m, ui.profDcCeiling,
			   ui.profPhe, ui.profPn2, ui.profPO2, // partial pressure graphs
			   ui.profRuler, ui.profScaled, // measuring and scaling
			   ui.profTogglePicture, ui.profTankbar,
			   ui.profMod, ui.profDeco, ui.profNdl_tts, // various values that a user is either interested in or not
			   ui.profEad, ui.profSAC,
			   ui.profHR, // very few dive computers support this
			   ui.profTissues }; // maybe less frequently used

	view.reset(new ProfileWidget2(DivePlannerPointsModel::instance(), 1.0, this));
	QToolBar *toolBar = new QToolBar(this);
	for (QAction *a: toolbarActions)
		toolBar->addAction(a);
	toolBar->setOrientation(Qt::Vertical);
	toolBar->setIconSize(QSize(24, 24));
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setSpacing(0);
	layout->setMargin(0);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(toolBar);
	layout->addWidget(view.get());
	setLayout(layout);

	// Toolbar Connections related to the Profile Update
	auto tec = qPrefTechnicalDetails::instance();
	connect(ui.profCalcAllTissues, &QAction::triggered, tec, &qPrefTechnicalDetails::set_calcalltissues);
	connect(ui.profCalcCeiling,    &QAction::triggered, tec, &qPrefTechnicalDetails::set_calcceiling);
	connect(ui.profDcCeiling,      &QAction::triggered, tec, &qPrefTechnicalDetails::set_dcceiling);
	connect(ui.profEad,            &QAction::triggered, tec, &qPrefTechnicalDetails::set_ead);
	connect(ui.profIncrement3m,    &QAction::triggered, tec, &qPrefTechnicalDetails::set_calcceiling3m);
	connect(ui.profMod,            &QAction::triggered, tec, &qPrefTechnicalDetails::set_mod);
	connect(ui.profNdl_tts,        &QAction::triggered, tec, &qPrefTechnicalDetails::set_calcndltts);
	connect(ui.profDeco,           &QAction::triggered, tec, &qPrefTechnicalDetails::set_decoinfo);
	connect(ui.profHR,             &QAction::triggered, tec, &qPrefTechnicalDetails::set_hrgraph);
	connect(ui.profRuler,          &QAction::triggered, tec, &qPrefTechnicalDetails::set_rulergraph);
	connect(ui.profSAC,            &QAction::triggered, tec, &qPrefTechnicalDetails::set_show_sac);
	connect(ui.profScaled,         &QAction::triggered, tec, &qPrefTechnicalDetails::set_zoomed_plot);
	connect(ui.profTogglePicture,  &QAction::triggered, tec, &qPrefTechnicalDetails::set_show_pictures_in_profile);
	connect(ui.profTankbar,        &QAction::triggered, tec, &qPrefTechnicalDetails::set_tankbar);
	connect(ui.profTissues,        &QAction::triggered, tec, &qPrefTechnicalDetails::set_percentagegraph);

	connect(ui.profTissues,        &QAction::triggered, this, &ProfileWidget::unsetProfHR);
	connect(ui.profHR,             &QAction::triggered, this, &ProfileWidget::unsetProfTissues);

	auto pp_gas = qPrefPartialPressureGas::instance();
	connect(ui.profPhe, &QAction::triggered, pp_gas, &qPrefPartialPressureGas::set_phe);
	connect(ui.profPn2, &QAction::triggered, pp_gas, &qPrefPartialPressureGas::set_pn2);
	connect(ui.profPO2, &QAction::triggered, pp_gas, &qPrefPartialPressureGas::set_po2);

	connect(&diveListNotifier, &DiveListNotifier::settingsChanged, view.get(), &ProfileWidget2::settingsChanged);

	ui.profCalcAllTissues->setChecked(qPrefTechnicalDetails::calcalltissues());
	ui.profCalcCeiling->setChecked(qPrefTechnicalDetails::calcceiling());
	ui.profDcCeiling->setChecked(qPrefTechnicalDetails::dcceiling());
	ui.profEad->setChecked(qPrefTechnicalDetails::ead());
	ui.profIncrement3m->setChecked(qPrefTechnicalDetails::calcceiling3m());
	ui.profMod->setChecked(qPrefTechnicalDetails::mod());
	ui.profNdl_tts->setChecked(qPrefTechnicalDetails::calcndltts());
	ui.profDeco->setChecked(qPrefTechnicalDetails::decoinfo());
	ui.profPhe->setChecked(pp_gas->phe());
	ui.profPn2->setChecked(pp_gas->pn2());
	ui.profPO2->setChecked(pp_gas->po2());
	ui.profHR->setChecked(qPrefTechnicalDetails::hrgraph());
	ui.profRuler->setChecked(qPrefTechnicalDetails::rulergraph());
	ui.profSAC->setChecked(qPrefTechnicalDetails::show_sac());
	ui.profTogglePicture->setChecked(qPrefTechnicalDetails::show_pictures_in_profile());
	ui.profTankbar->setChecked(qPrefTechnicalDetails::tankbar());
	ui.profTissues->setChecked(qPrefTechnicalDetails::percentagegraph());
	ui.profScaled->setChecked(qPrefTechnicalDetails::zoomed_plot());
}

ProfileWidget::~ProfileWidget()
{
}

void ProfileWidget::setEnabledToolbar(bool enabled)
{
	for (QAction *b: toolbarActions)
		b->setEnabled(enabled);
}

void ProfileWidget::setDive(const struct dive *d)
{
	stack->setCurrentIndex(1); // show profile

	bool freeDiveMode = d->dc.divemode == FREEDIVE;
	ui.profCalcCeiling->setDisabled(freeDiveMode);
	ui.profCalcCeiling->setDisabled(freeDiveMode);
	ui.profCalcAllTissues ->setDisabled(freeDiveMode);
	ui.profIncrement3m->setDisabled(freeDiveMode);
	ui.profDcCeiling->setDisabled(freeDiveMode);
	ui.profPhe->setDisabled(freeDiveMode);
	ui.profPn2->setDisabled(freeDiveMode); //TODO is the same as scuba?
	ui.profPO2->setDisabled(freeDiveMode); //TODO is the same as scuba?
	ui.profTankbar->setDisabled(freeDiveMode);
	ui.profMod->setDisabled(freeDiveMode);
	ui.profNdl_tts->setDisabled(freeDiveMode);
	ui.profDeco->setDisabled(freeDiveMode);
	ui.profEad->setDisabled(freeDiveMode);
	ui.profSAC->setDisabled(freeDiveMode);
	ui.profTissues->setDisabled(freeDiveMode);

	ui.profRuler->setDisabled(false);
	ui.profScaled->setDisabled(false); // measuring and scaling
	ui.profTogglePicture->setDisabled(false);
	ui.profHR->setDisabled(false);
}

void ProfileWidget::plotCurrentDive()
{
	setEnabledToolbar(current_dive != nullptr);
	if (current_dive)
		setDive(current_dive);
	view->plotDive(current_dive, dc_number);
}

void ProfileWidget::setPlanState(const struct dive *d, int dc)
{
	setDive(d); // show subsurface logo
	view->setPlanState(d, dc);
}

void ProfileWidget::setEditState(const struct dive *d, int dc)
{
	setDive(d);
	view->setEditState(d, dc);
}

void ProfileWidget::unsetProfHR()
{
	ui.profHR->setChecked(false);
	qPrefTechnicalDetails::set_hrgraph(false);
}

void ProfileWidget::unsetProfTissues()
{
	ui.profTissues->setChecked(false);
	qPrefTechnicalDetails::set_percentagegraph(false);
}
