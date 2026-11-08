/*******************************************************************************
 * plugins/RsWiki/gui/WikiModeratorChooserDialog.h                             *
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

#ifndef RSWIKI_MODERATOR_CHOOSER_DIALOG_H
#define RSWIKI_MODERATOR_CHOOSER_DIALOG_H

#include <QDialog>

#include <set>

#include <retroshare/rsgxsifacetypes.h>

#include "ui_RsWikiModeratorChooserDialog.h"

/* Lets the group admin pick GXS identities to make moderators.
 * The layout lives in WikiModeratorChooserDialog.ui. */
namespace rswiki {

class WikiModeratorChooserDialog : public QDialog
{
	Q_OBJECT

public:
	WikiModeratorChooserDialog(const std::set<RsGxsId> &currentModerators, QWidget *parent = nullptr);

	/* Identities checked in the list when the dialog was accepted */
	std::set<RsGxsId> selectedIds() const;

private slots:
	void filterChanged(int index);

private:
	::Ui::RsWikiModeratorChooserDialog ui;
};

} // namespace rswiki

#endif
