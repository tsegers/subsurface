// SPDX-License-Identifier: GPL-2.0
#ifndef DIVETEXTITEM_H
#define DIVETEXTITEM_H

#include <QObject>
#include <QFont>
#include <QGraphicsItemGroup>

class QBrush;

/* A Line Item that has animated-properties. */
class DiveTextItem : public QObject, public QGraphicsItemGroup {
	Q_OBJECT
	Q_PROPERTY(QPointF pos READ pos WRITE setPos)
	Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
public:
	DiveTextItem(double dpr, double scale, int alignFlags, QGraphicsItem *parent = 0);
	void set(const QString &text, const QBrush &brush);
	const QString &text();
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	static QFont getFont(double dpr, double scale);
	static double fontHeight(double dpr, double scale);
	double height() const;

private:
	void updateText();
	int internalAlignFlags;
	QGraphicsPathItem *textBackgroundItem;
	QGraphicsPathItem *textItem;
	QString internalText;
	double dpr;
	double scale;
};

#endif // DIVETEXTITEM_H
