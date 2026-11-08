/*******************************************************************************
 * plugins/RsWiki/gui/WikiCommentItem.cpp                                      *
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

#include "WikiCommentItem.h"
#include "WikiStyle.h"

#include "gui/gxs/GxsIdDetails.h"
#include "util/misc.h"

namespace rswiki {

WikiCommentItem::WikiCommentItem(const RsWikiComment &comment, QWidget *parent)
	: QFrame(parent)
{
	ui.setupUi(this);

	const RsGxsId &author = comment.mMeta.mAuthorId;
	ui.commentAuthorLabel->setId(author);
	ui.commentAvatarLabel->setPixmap(GxsIdDetails::makeDefaultIcon(author, GxsIdDetails::SMALL));

	ui.commentTimeLabel->setText(misc::timeRelativeToNow(comment.mMeta.mPublishTs));
	ui.commentTimeLabel->setToolTip(friendlyTime(comment.mMeta.mPublishTs));

	ui.commentTextLabel->setText(QString::fromUtf8(comment.mComment.c_str()).toHtmlEscaped()
	                             .replace('\n', "<br/>"));
}

} // namespace rswiki
