/*******************************************************************************
 * plugins/RsWiki/gui/WikiSpaceCard.h                                          *
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

#ifndef RSWIKI_SPACE_CARD_H
#define RSWIKI_SPACE_CARD_H

#include <QFrame>

#include "ui_RsWikiSpaceCard.h"

class QMouseEvent;

namespace rswiki {

/* A clickable card for one wiki on the home page (RsWikiSpaceCard.ui) */
class WikiSpaceCard : public QFrame
{
	Q_OBJECT

public:
	explicit WikiSpaceCard(QWidget *parent = nullptr);

	void setWiki(const QString &groupId, const QString &name, int pageCount, int subscribers);
	QString groupId() const { return mGroupId; }

signals:
	void clicked(const QString &groupId);

protected:
	virtual void mouseReleaseEvent(QMouseEvent *event) override;

private:
	QString mGroupId;
	::Ui::RsWikiSpaceCard ui;
};

} // namespace rswiki

#endif
