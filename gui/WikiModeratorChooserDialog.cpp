/*******************************************************************************
 * plugins/RsWiki/gui/WikiModeratorChooserDialog.cpp                           *
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

#include "WikiModeratorChooserDialog.h"

#include "gui/common/FriendSelectionWidget.h"

namespace rswiki {

WikiModeratorChooserDialog::WikiModeratorChooserDialog(const std::set<RsGxsId> &currentModerators, QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	ui.friendSelectionWidget->setHeaderText(tr("Moderators:"));
	ui.friendSelectionWidget->setModus(FriendSelectionWidget::MODUS_CHECK);
	ui.friendSelectionWidget->setShowType(FriendSelectionWidget::SHOW_GXS);
	ui.friendSelectionWidget->start();
	ui.friendSelectionWidget->setSelectedIds<RsGxsId,FriendSelectionWidget::IDTYPE_GXS>(currentModerators, false);

	ui.filterComboBox->setCurrentIndex(0);
	connect(ui.filterComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(filterChanged(int)));
}

void WikiModeratorChooserDialog::filterChanged(int index)
{
	switch (index)
	{
	default:
	case 0:
		ui.friendSelectionWidget->setShowType(FriendSelectionWidget::SHOW_GXS);
		break;
	case 1:
		ui.friendSelectionWidget->setShowType(FriendSelectionWidget::SHOW_CONTACTS);
		break;
	}
}

std::set<RsGxsId> WikiModeratorChooserDialog::selectedIds() const
{
	std::set<RsGxsId> ids;
	ui.friendSelectionWidget->selectedIds<RsGxsId,FriendSelectionWidget::IDTYPE_GXS>(ids, true);
	return ids;
}

} // namespace rswiki
