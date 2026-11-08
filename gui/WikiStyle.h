/*******************************************************************************
 * plugins/RsWiki/gui/WikiStyle.h                                              *
 *                                                                             *
 * Copyright (C) 2026 RetroShare Team <retroshare.project@gmail.com>           *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Affero General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Affero General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Affero General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

#ifndef RSWIKI_STYLE_H
#define RSWIKI_STYLE_H

#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QLocale>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <ctime>

/* Small look helpers shared by the RsWiki widgets. Layouts live in the .ui
 * files and colours come from the theme; these only produce the per-wiki
 * accent colours, the initials avatars and friendly dates. */
namespace rswiki {

/* A stable accent colour per wiki (or identity), from its id */
inline QColor wikiColor(const QString &seed)
{
	static const char *colors[] = {
		"#4F46E5", "#BE185D", "#0F766E", "#7E22CE",
		"#B45309", "#1D4ED8", "#B91C1C", "#047857"
	};
	uint h = 0;
	for (const QChar &c : seed)
		h = h * 31 + c.unicode();
	return QColor(colors[h % (sizeof(colors) / sizeof(colors[0]))]);
}

/* Up to two initials from a name, "RetroShare Docs" -> "RD" */
inline QString wikiInitials(const QString &name)
{
	const QStringList words = name.simplified().split(' ');
	QString initials;
	for (const QString &word : words)
	{
		if (initials.size() == 2)
			break;
		if (!word.isEmpty())
			initials += word.at(0).toUpper();
	}
	if (initials.size() == 1 && name.simplified().size() > 1)
		initials += name.simplified().at(1).toUpper();
	return initials.isEmpty() ? QString("?") : initials;
}

/* A round avatar with initials on the wiki colour */
inline QPixmap wikiAvatar(const QString &name, const QString &seed, int size)
{
	const qreal dpr = 2.0;
	QPixmap pix(int(size * dpr), int(size * dpr));
	pix.setDevicePixelRatio(dpr);
	pix.fill(Qt::transparent);

	QPainter p(&pix);
	p.setRenderHint(QPainter::Antialiasing);
	p.setPen(Qt::NoPen);
	p.setBrush(wikiColor(seed));
	p.drawEllipse(QRectF(0, 0, size, size));

	QFont f;
	f.setBold(true);
	f.setPixelSize(qMax(8, int(size * 0.38)));
	p.setFont(f);
	p.setPen(Qt::white);
	p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, wikiInitials(name));
	return pix;
}

/* "Today, 11:02", "Yesterday, 18:15" or a date */
inline QString friendlyTime(time_t t)
{
	if (t <= 0)
		return QString();
	const QDateTime dt = QDateTime::fromSecsSinceEpoch(t);
	const QDate today = QDate::currentDate();
	const QString time = dt.time().toString("HH:mm");
	if (dt.date() == today)
		return QObject::tr("Today, %1").arg(time);
	if (dt.date() == today.addDays(-1))
		return QObject::tr("Yesterday, %1").arg(time);
	return QLocale().toString(dt.date(), QLocale::ShortFormat);
}

} // namespace rswiki

#endif // RSWIKI_STYLE_H
