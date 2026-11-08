/*******************************************************************************
 * plugins/RsWiki/gui/WikiSpaceCard.cpp                                        *
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

#include "WikiSpaceCard.h"
#include "WikiStyle.h"

#include <QMouseEvent>

namespace rswiki {

WikiSpaceCard::WikiSpaceCard(QWidget *parent)
	: QFrame(parent)
{
	ui.setupUi(this);
	setAttribute(Qt::WA_Hover);
}

void WikiSpaceCard::setWiki(const QString &groupId, const QString &name, int pageCount, int subscribers)
{
	mGroupId = groupId;

	ui.cardNameLabel->setText(name);
	ui.cardStatsLabel->setText(tr("%n page(s)", "", pageCount) + QString::fromUtf8(" · ")
	                           + tr("%n subscriber(s)", "", subscribers));
	ui.cardAvatarLabel->setPixmap(wikiAvatar(name, groupId, 40));
	/* the wiki colour, translucent so it sits on any theme background */
	const QColor c = wikiColor(groupId);
	ui.cardBannerFrame->setStyleSheet(
		QString("background: rgba(%1, %2, %3, 70); border: none; border-top-left-radius: 9px; border-top-right-radius: 9px;")
			.arg(c.red()).arg(c.green()).arg(c.blue()));
	setToolTip(name);
}

void WikiSpaceCard::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton && rect().contains(event->pos()))
		emit clicked(mGroupId);

	QFrame::mouseReleaseEvent(event);
}

} // namespace rswiki
