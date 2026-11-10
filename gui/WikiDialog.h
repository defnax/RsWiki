/*******************************************************************************
 * plugins/RsWiki/gui/WikiDialog.h                                             *
 *                                                                             *
 * Copyright 2012-2012 by Robert Fernie <retroshare@lunamutt.com>              *
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

#ifndef MRK_WIKI_DIALOG_H
#define MRK_WIKI_DIALOG_H

#include <map>
#include <memory>

#include <QPointer>
#include <set>
#include <vector>

#include "retroshare-gui/mainpage.h"
#include "ui_RsWikiDialog.h"

#include "interface/rsWiki.h"

#define IMAGE_WIKI              ":/icons/png/wiki.png"

class UserNotify;
class QTreeWidget;
class QTreeWidgetItem;
class QListWidgetItem;
class QMenu;
class QTimer;

namespace rswiki {

class WikiEditDialog;

/*
 * The RsWiki main page (RsWikiDialog.ui).
 *
 * Left: a sidebar that shows either the home navigation (Home, Favorites,
 * Browse, my wikis) or, inside a wiki, the wiki switcher and its page tree.
 * Right: one of four pages - Home (wiki cards and page lists), Browse (all
 * wikis), Overview (one wiki) and Page (a page with its comments).
 */
class WikiDialog : public MainPage
{
	Q_OBJECT

public:
	WikiDialog(QWidget *parent = nullptr);
	~WikiDialog();

	virtual QIcon iconPixmap() const override { return QIcon(IMAGE_WIKI); }
	virtual QString pageName() const override { return tr("RsWiki"); }
	virtual QString helpText() const override { return ""; }

	virtual UserNotify *createUserNotify(QObject *parent) override;

protected:
	virtual void showEvent(QShowEvent *event) override;
	virtual void changeEvent(QEvent *event) override;

private slots:
	/* Navigation */
	void showHome();
	void showFavorites();
	void showBrowse();
	void showOverview();
	void openSpaceFromId(const QString &groupId);
	void myWikiClicked(QListWidgetItem *item);
	void pageTreeClicked(QTreeWidgetItem *item, int column);
	void pageListActivated(QTreeWidgetItem *item, int column);
	void browseActivated(QTreeWidgetItem *item, int column);
	void breadcrumbLinkActivated(const QString &link);
	void noSpacesLinkActivated(const QString &link);
	void pageTagLinkActivated(const QString &link);
	void myWikisContextMenu(const QPoint &point);
	void markCurrentWikiRead();
	void overviewLinkActivated(const QString &link);

	/* Home */
	void listTabChanged();
	void homeSearchChanged();

	/* Wiki actions */
	void createWiki();
	void openSpaceSettings();
	void toggleSubscribe();
	void browseSubscribe();
	void browseOpen();
	void browseFilterChanged();
	void browseSelectionChanged();
	void newPage();
	void newSubPage();
	void filterPageTree();

	/* Page actions */
	void editPage();
	void showPageHistory();
	void republishPage();
	void favoriteToggled(bool checked);
	void commentsToggled(bool checked);
	void markCurrentPageUnread();
	void postComment();

	/* Context menus */
	void pageTreeContextMenu(const QPoint &point);
	void pageListContextMenu(const QPoint &point);
	void overviewPagesContextMenu(const QPoint &point);
	void browseContextMenu(const QPoint &point);

	/* Data */
	void reloadAll();

private:
	enum ContentPage { PAGE_HOME = 0, PAGE_BROWSE = 1, PAGE_OVERVIEW = 2, PAGE_PAGE = 3 };
	enum ListMode { LIST_RECENT, LIST_FAVORITES, LIST_MINE };

	/* data helpers */
	static RsGxsMessageId pageOrigId(const RsWikiSnapshot &page);
	const RsWikiCollection *findCollection(const RsGxsGroupId &groupId) const;
	const RsWikiSnapshot *findPage(const RsGxsGroupId &groupId, const RsGxsMessageId &origId) const;
	bool isFollowed(const RsWikiCollection &collection) const;
	bool canModerate(const RsGxsGroupId &groupId) const;
	int unreadCount(const RsGxsGroupId &groupId) const;
	/* unread comments and edit proposals of a page (GXS child messages) */
	uint32_t unreadChildCount(const RsWikiSnapshot &page) const;
	/* the page itself or anything posted on it is unread */
	bool pageHasUnread(const RsWikiSnapshot &page) const;
	QString authorName(const RsGxsId &id) const;
	std::vector<const RsWikiCollection *> followedCollections() const;

	/* favorites (kept locally in the settings) */
	static QString favoriteKey(const RsGxsGroupId &groupId, const RsGxsMessageId &origId);
	bool isFavorite(const RsGxsGroupId &groupId, const RsGxsMessageId &origId) const;
	void setFavorite(const RsGxsGroupId &groupId, const RsGxsMessageId &origId, bool favorite);

	/* refresh the views from the loaded data */
	void refreshAll();
	void fillMyWikis();
	void fillSpaceCards();
	void fillPageList();
	void fillBrowse();
	void fillSpaceSidebar();
	void fillPageTree();
	void fillOverview();
	void updatePageActions();

	/* navigation */
	void openSpace(const RsGxsGroupId &groupId, bool showOverviewPage);
	void openPage(const RsGxsGroupId &groupId, const RsGxsMessageId &origId);
	void renderPage(const RsWikiSnapshot &page);
	void setContentPage(ContentPage page);
	void showBrowseFor(const QString &category, const QString &filter);

	/* pages and comments */
	void startNewPage(const RsGxsMessageId &parentOrigId);
	void openEditor(bool history);
	void markPageRead(const RsWikiSnapshot &page, bool read);
	void markPageThreadRead(const RsGxsGroupId &groupId, const RsGxsMessageId &origId);
	void markWikiRead(const RsGxsGroupId &groupId);
	void loadComments();
	void clearComments();
	void showPageContextMenu(const RsGxsGroupId &groupId, const RsGxsMessageId &origId, bool inCurrentWiki);

	/* items */
	QTreeWidgetItem *makePageListItem(const RsWikiSnapshot &page) const;
	static void setItemPage(QTreeWidgetItem *item, const RsGxsGroupId &groupId, const RsGxsMessageId &origId);
	static bool itemPage(const QTreeWidgetItem *item, RsGxsGroupId &groupId, RsGxsMessageId &origId);

	/* light skin (gui/qss/RsWiki_light.qss) on top of the .ui stylesheet */
	void applySkin();
	static bool isLightTheme();
	static QString lightSkin();

	void scheduleReload();
	void handleEvent_main_thread(std::shared_ptr<const RsEvent> event);
	void processSettings(bool load);

	/* loaded data */
	std::vector<RsWikiCollection> mCollections;
	std::map<RsGxsGroupId, std::vector<RsWikiSnapshot>> mPages;   /* latest version of each page */
	std::set<RsGxsId> mOwnIds;
	std::set<RsGxsGroupId> mModeratedGroups;
	std::map<RsGxsGroupId, std::map<RsGxsMessageId, uint32_t>> mUnreadChildren;
	QPointer<UserNotify> mUserNotify;
	QStringList mFavorites;

	/* navigation state */
	RsGxsGroupId mCurrentGroupId;
	RsGxsMessageId mCurrentPageOrigId;
	RsGxsMessageId mCurrentPageMsgId;
	RsGxsMessageId mCommentsLoadedFor;
	ListMode mListMode;

	WikiEditDialog *mEditDialog;
	QMenu *mSpaceMenu;
	QMenu *mMoreMenu;
	QTimer *mReloadTimer;
	uint32_t mEventHandlerId;
	bool mInitialLoadDone;
	bool mLoading;
	bool mApplyingSkin;
	QString mBaseStyleSheet;

	/* UI - from Designer */
	::Ui::RsWikiDialog ui;
};

} // namespace rswiki

#endif
