// SPDX-License-Identifier: GPL-2.0
#include "divepercentageitem.h"
#include "divecartesianaxis.h"
#include "core/dive.h"
#include "core/profile.h"

#include <array>

DivePercentageItem::DivePercentageItem(const DiveCartesianAxis &hAxis, const DiveCartesianAxis &vAxis, double dpr) :
	hAxis(hAxis), vAxis(vAxis), dpr(dpr)
{
}

static const constexpr int num_tissues = 16;

// Calculate the number of scanlines for every drawn tissue.
static std::array<int, num_tissues> calcLinesPerTissue(int size)
{
	std::array<int, num_tissues> res;

	// A Bresenham-inspired algorithm without the weird half steps at the beginning and the end.
	if (size <= 0) {
		std::fill(res.begin(), res.end(), 0);
	} else if (size >= num_tissues) {
		int step = size / num_tissues;
		int err_inc = size % num_tissues;
		int err = 0;
		for (int i = 0; i < num_tissues; ++i) {
			res[i] = step;
			err += err_inc;
			if (err >= num_tissues) {
				err -= num_tissues;
				++res[i];
			}
		}
	} else { // size < num_tissues
		int step = num_tissues / size;
		int err_inc = num_tissues % size;
		int err = 0;
		int act = 0;
		std::fill(res.begin(), res.end(), 0);
		for (int i = 0; i < size; ++i) {
			res[act] = 1;
			act += step;
			err += err_inc;
			if (err >= size) {
				err -= size;
				++act;
			}
		}
	}
	return res;
}

static QRgb colorScale(double value, int inert)
{
	QColor color;
	double scaledValue = value / (AMB_PERCENTAGE * inert) * 1000.0;
	if (scaledValue < 0.8)	// grade from cyan to blue to purple
		color.setHsvF(0.5 + 0.25 * scaledValue / 0.8, 1.0, 1.0);
	else if (scaledValue < 1.0)	// grade from magenta to black
		color.setHsvF(0.75, 1.0, (1.0 - scaledValue) / 0.2);
	else if (value < AMB_PERCENTAGE)	// grade from black to bright green
		color.setHsvF(0.333, 1.0, (value - AMB_PERCENTAGE * inert / 1000.0) / (AMB_PERCENTAGE - AMB_PERCENTAGE * inert / 1000.0));
	else if (value < 65)		// grade from bright green (0% M) to yellow-green (30% M)
		color.setHsvF(0.333 - 0.133 * (value - AMB_PERCENTAGE) / (65.0 - AMB_PERCENTAGE), 1.0, 1.0);
	else if (value < 85)		// grade from yellow-green (30% M) to orange (70% M)
		color.setHsvF(0.2 - 0.1 * (value - 65.0) / 20.0, 1.0, 1.0);
	else if (value < 100)		// grade from orange (70% M) to red (100% M)
		color.setHsvF(0.1 * (100.0 - value) / 15.0, 1.0, 1.0);
	else if (value < 120)		// M value exceeded - grade from red to white
		color.setHsvF(0.0, 1 - (value - 100.0) / 20.0, 1.0);
	else	// white
		color.setHsvF(0.0, 0.0, 1.0);
	return color.rgba();
}

void DivePercentageItem::replot(const dive *d, const struct divecomputer *dc, const plot_info &pi)
{
	auto [minX, maxX] = hAxis.screenMinMax();
	auto [minY, maxY] = vAxis.screenMinMax();
	int width = lrint(maxX) - lrint(minX);
	int height = lrint(maxY) - lrint(minY);
	if (width <= 0 || height <= 0) {
		setPixmap(QPixmap());
		return;
	}

	std::array<int, num_tissues> linesPerTissue = calcLinesPerTissue(height);

	QImage img(width, height, QImage::QImage::Format_ARGB32);

	int line = 0;
	for (int tissue = 0; tissue < num_tissues; ++tissue) {
		if (linesPerTissue[tissue] <= 0)
			continue;
		int x = 0;
		QRgb *scanline = (QRgb *)img.scanLine(line);
		QRgb color = 0;
		const struct event *ev = NULL;
		for (int i = 0; i < pi.nr; i++) {
			const plot_data &item = pi.entry[i];
			int sec = item.sec;
			int nextX = lrint(hAxis.posAtValue(sec)) - lrint(minX);
			if (nextX == x)
				continue;

			double value = item.percentages[tissue];
			struct gasmix gasmix = get_gasmix(d, dc, sec, &ev, gasmix);
			int inert = get_n2(gasmix) + get_he(gasmix);
			color = colorScale(value, inert);
			if (nextX >= width)
				nextX = width - 1;
			for (; x <= nextX; ++x)
				scanline[x] = color;
			if (nextX >= width - 1)
				break;
		}
		for (; x < width; ++x)
			scanline[x] = color;
		++line;

		// Clone line if needed
		for (int i = 0; i < linesPerTissue[tissue] - 1; ++i) {
			QRgb *scanline2 = (QRgb *)img.scanLine(line);
			std::copy(scanline, scanline + width, scanline2);
			++line;
		}
	}
	setPixmap(QPixmap::fromImage(img));
	setPos(minX, minY);
}
