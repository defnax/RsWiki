/*******************************************************************************
 * plugins/RsWiki/gui/WikiEditDialog.h                                         *
 *                                                                             *
 * Copyright (C) 2012 Robert Fernie   <retroshare.project@gmail.com>           *
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

#ifndef MRK_WIKI_EDIT_DIALOG_H
#define MRK_WIKI_EDIT_DIALOG_H

#include "ui_RsWikiEditDialog.h"

#include <map>
#include <vector>

#include "interface/rsWiki.h"

class RSTreeWidgetItemCompareRole;
class QTreeWidgetItem;

namespace rswiki {

class WikiEditDialog : public QWidget
{
  Q_OBJECT

public:
	WikiEditDialog(QWidget *parent = 0);
	~WikiEditDialog();

void 	setNewPage();

void 	setupData(const RsGxsGroupId &groupId, const RsGxsMessageId &pageId);

void 	setRepublishMode(RsGxsMessageId &origMsgId);

/* Submit publishes a new version of the page (origMsgId) instead of an edit
 * proposal. For the page author and moderators: peers only accept it from
 * them, so the editor signs with publisherId. currentMsgId is the version
 * shown now; submitting it unchanged is pointless. */
void 	setDirectPublish(const RsGxsMessageId &origMsgId, const RsGxsMessageId &currentMsgId,
	                 const RsGxsId &publisherId);

/* For a new page: make it a sub-page of this page (original message id) */
void 	setParentPage(const RsGxsMessageId &parentPageId);

/* Opens the edit history pane */
void 	showHistory();

private slots:

void 	cancelEdit();
void 	revertEdit();
void 	submitEdit();
void 	previewToggle();
void 	historyToggle();
void  	detailsToggle();
void 	textChanged();
void    textReset();

void 	historySelected();
void 	oldHistoryChanged();
void  	mergeModeToggle();
void  	generateMerge();
void    historyItemChanged(QTreeWidgetItem *item, int column);
void    updateMergeButtonState();

private:

void 	updateHistoryStatus();
void 	updateHistoryChildren(QTreeWidgetItem *item, bool isLatest);
void 	updateHistoryItem(QTreeWidgetItem *item, bool isLatest);
std::vector<RsGxsMessageId> collectMergeSelection() const;
void    rebuildMergeSelectionCount();

void    redrawPage();

void 	setGroup(const RsWikiCollection &group);
void 	setPreviousPage(const RsWikiSnapshot &page);

void 	requestPage(const RsGxsGrpMsgIdPair &msgId);
void 	loadPage(const RsWikiSnapshot &page);
void 	requestGroup(const RsGxsGroupId &groupId);
void 	loadGroup(const RsWikiCollection &group);

void 	requestBaseHistory(const RsGxsGrpMsgIdPair &origMsgId);
void 	loadBaseHistory(const std::vector<RsWikiSnapshot> &snapshots);
void 	requestEditTreeData();
void 	loadEditTreeData(const std::vector<RsWikiSnapshot> &snapshots);

void 	performMerge(
		const std::vector<RsGxsMessageId> &editIds,
		const std::map<RsGxsMessageId, std::string> &contents);
rstime_t getEditTimestamp(const RsGxsMessageId &msgId) const;
QString getAuthorName(const RsGxsMessageId &msgId) const;
QTreeWidgetItem *findHistoryItem(const RsGxsMessageId &msgId) const;
void 	resetEditorState();


        bool mNewPage;

	bool mPreviewMode;
	bool mPageLoading;
        bool mHistoryLoaded;
        bool mHistoryMergeMode;
        bool mOldHistoryEnabled;

        bool mRepublishMode;
        bool mDirectPublish;
        RsGxsMessageId mDirectBaseMsgId;
	bool mIgnoreTextChange; // when we do it programmatically.
        bool mTextChanged;
	int mCheckedMergeCount;

	QString mCurrentText;

	RsGxsGrpMsgIdPair mThreadMsgIdPair;
	RsGxsMessageId mRepublishOrigId;

	RsWikiCollection mWikiCollection;
	RsWikiSnapshot mWikiSnapshot;

	::Ui::RsWikiEditDialog ui;

	RSTreeWidgetItemCompareRole *mThreadCompareRole;
};

} // namespace rswiki

#endif
