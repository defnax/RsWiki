/*******************************************************************************
 * plugins/RsWiki/gui/WikiDialog.cpp                                           *
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

#include <QApplication>
#include <QCursor>
#include <QDateTime>
#include <QFile>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QTreeWidgetItemIterator>
#include <QUrl>

#include <algorithm>
#include <ctime>
#include <functional>

#include "WikiDialog.h"
#include "WikiCommentItem.h"
#include "WikiEditDialog.h"
#include "WikiGroupDialog.h"
#include "WikiSpaceCard.h"
#include "WikiStyle.h"
#include "WikiUserNotify.h"

#include "gui/settings/rsharesettings.h"
#include "util/HandleRichText.h"
#include "util/RsMarkdown.h"
#include "util/qtthreadsutils.h"

#include <retroshare/rsgxsflags.h>
#include <retroshare/rsidentity.h>
#include <retroshare/rsinit.h>
#include "util/rsthreads.h"

namespace rswiki {

namespace {

const int ROLE_GROUP = Qt::UserRole;
const int ROLE_ORIG  = Qt::UserRole + 1;
const int ROLE_SORT  = Qt::UserRole + 2;

const int CARDS_PER_ROW  = 4;
const int MAX_CARDS      = 8;
const int MAX_LIST_ITEMS = 100;

const char SETTINGS_GROUP[] = "RsWikiDialog";

inline QString toQ(const std::string &s) { return QString::fromUtf8(s.c_str()); }

inline bool isUnread(uint32_t status) { return IS_MSG_UNREAD(status) || IS_MSG_NEW(status); }

const int ROLE_TAGS = Qt::UserRole + 3;

/* "#tag" matches a tag exactly ("#" alone: any tagged page/wiki); other text
 * matches any of the given texts or a tag containing it */
bool textMatches(const QString &search, const QStringList &texts, const QStringList &tags)
{
	if (search.startsWith('#'))
	{
		const QString tag = search.mid(1).trimmed();
		return tag.isEmpty() ? !tags.isEmpty() : tags.contains(tag, Qt::CaseInsensitive);
	}

	for (const QString &text : texts)
	{
		if (text.contains(search, Qt::CaseInsensitive))
			return true;
	}
	for (const QString &tag : tags)
	{
		if (tag.contains(search, Qt::CaseInsensitive))
			return true;
	}
	return false;
}

QString categoryLink(const QString &category)
{
	return QString("<a href=\"category:%1\" style=\"text-decoration:none\">%2</a>")
		.arg(QString::fromLatin1(QUrl::toPercentEncoding(category)), category.toHtmlEscaped());
}

/* Tree item that sorts on ROLE_SORT (timestamps, counts) when it is set */
class SortItem : public QTreeWidgetItem
{
public:
	SortItem() : QTreeWidgetItem(UserType) {}

	bool operator<(const QTreeWidgetItem &other) const override
	{
		const int column = treeWidget() ? treeWidget()->sortColumn() : 0;
		const QVariant a = data(column, ROLE_SORT);
		const QVariant b = other.data(column, ROLE_SORT);
		if (a.isValid() && b.isValid())
			return a.toLongLong() < b.toLongLong();
		return text(column).toLower() < other.text(column).toLower();
	}
};

void setBold(QTreeWidgetItem *item, int column, bool bold)
{
	QFont f = item->font(column);
	f.setBold(bold);
	item->setFont(column, f);
}

} // namespace

/*********************************************************************************
 * Construction
 *********************************************************************************/

WikiDialog::WikiDialog(QWidget *parent)
	: MainPage(parent)
	, mListMode(LIST_RECENT)
	, mEditDialog(nullptr)
	, mSpaceMenu(nullptr)
	, mMoreMenu(nullptr)
	, mReloadTimer(nullptr)
	, mEventHandlerId(0)
	, mInitialLoadDone(false)
	, mLoading(false)
	, mApplyingSkin(false)
{
	/* Invoke the Qt Designer generated object setup routine */
	ui.setupUi(this);

	/* The .ui stylesheet only sets fonts and spacing; colours come from the
	 * theme, plus the light skin while a light theme is active */
	mBaseStyleSheet = styleSheet();
	applySkin();

	/* Column sizing */
	ui.pageListTree->header()->setStretchLastSection(false);
	ui.pageListTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui.pageListTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	ui.pageListTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

	ui.overviewPagesTree->header()->setStretchLastSection(false);
	ui.overviewPagesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui.overviewPagesTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	ui.overviewPagesTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	ui.overviewPagesTree->sortByColumn(2, Qt::DescendingOrder);

	ui.browseTree->header()->setStretchLastSection(false);
	/* Wiki, Category, Description, Pages, Subscribers, Last activity, Status */
	for (int col = 0; col < ui.browseTree->columnCount(); ++col)
		ui.browseTree->header()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
	ui.browseTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
	ui.browseTree->sortByColumn(5, Qt::DescendingOrder);

	ui.pageSplitter->setStretchFactor(0, 3);
	ui.pageSplitter->setStretchFactor(1, 1);

	ui.commentIdChooser->loadIds(IDCHOOSER_ID_REQUIRED, RsGxsId());
	ui.emptyListLabel->hide();

	/* Menus filled when they open */
	mSpaceMenu = new QMenu(this);
	ui.spaceSwitchButton->setMenu(mSpaceMenu);
	connect(mSpaceMenu, &QMenu::aboutToShow, this, [this]()
	{
		mSpaceMenu->clear();
		for (const RsWikiCollection *c : followedCollections())
		{
			const QString id = QString::fromStdString(c->mMeta.mGroupId.toStdString());
			const QString name = toQ(c->mMeta.mGroupName);
			QAction *a = mSpaceMenu->addAction(QIcon(wikiAvatar(name, id, 20)), name);
			connect(a, &QAction::triggered, this, [this, id]() { openSpaceFromId(id); });
		}
		mSpaceMenu->addSeparator();
		mSpaceMenu->addAction(QIcon(":/rswiki/images/grid.svg"), tr("Browse all wikis"), this, SLOT(showBrowse()));
	});

	mMoreMenu = new QMenu(this);
	ui.moreButton->setMenu(mMoreMenu);
	connect(mMoreMenu, &QMenu::aboutToShow, this, [this]()
	{
		mMoreMenu->clear();
		const bool mod = canModerate(mCurrentGroupId);
		QAction *a = mMoreMenu->addAction(QIcon(":/rswiki/images/plus.svg"), tr("Add sub-page"), this, SLOT(newSubPage()));
		a->setEnabled(mod);
		a = mMoreMenu->addAction(QIcon(":/rswiki/images/edit.svg"), tr("Republish as official version"),
		                         this, SLOT(republishPage()));
		a->setEnabled(mod);
		a->setToolTip(tr("Moderators publish a new official version of the page"));
		mMoreMenu->addAction(tr("Mark as unread"), this, SLOT(markCurrentPageUnread()));
		mMoreMenu->addSeparator();
		mMoreMenu->addAction(QIcon(":/rswiki/images/settings.svg"), tr("Wiki settings"), this, SLOT(openSpaceSettings()));
	});

	/* Reloads are coalesced: a burst of GXS events causes one reload */
	mReloadTimer = new QTimer(this);
	mReloadTimer->setSingleShot(true);
	mReloadTimer->setInterval(400);
	connect(mReloadTimer, &QTimer::timeout, this, &WikiDialog::reloadAll);

	/* Home sidebar */
	connect(ui.navHomeButton, &QToolButton::clicked, this, &WikiDialog::showHome);
	connect(ui.navFavoritesButton, &QToolButton::clicked, this, &WikiDialog::showFavorites);
	connect(ui.navBrowseButton, &QToolButton::clicked, this, &WikiDialog::showBrowse);
	connect(ui.createWikiButton, &QToolButton::clicked, this, &WikiDialog::createWiki);
	connect(ui.myWikisList, &QListWidget::itemClicked, this, &WikiDialog::myWikiClicked);

	/* Home page */
	connect(ui.homeSearchEdit, &QLineEdit::textChanged, this, &WikiDialog::homeSearchChanged);
	connect(ui.noSpacesLabel, &QLabel::linkActivated, this, &WikiDialog::noSpacesLinkActivated);
	connect(ui.browseAllButton, &QPushButton::clicked, this, &WikiDialog::showBrowse);
	connect(ui.recentTabButton, &QToolButton::clicked, this, &WikiDialog::listTabChanged);
	connect(ui.favoritesTabButton, &QToolButton::clicked, this, &WikiDialog::listTabChanged);
	connect(ui.mineTabButton, &QToolButton::clicked, this, &WikiDialog::listTabChanged);
	connect(ui.pageListTree, &QTreeWidget::itemClicked, this, &WikiDialog::pageListActivated);
	connect(ui.pageListTree, &QTreeWidget::customContextMenuRequested, this, &WikiDialog::pageListContextMenu);

	/* Browse page */
	connect(ui.browseFilterEdit, &QLineEdit::textChanged, this, &WikiDialog::browseFilterChanged);
	connect(ui.browseCategoryCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(browseFilterChanged()));
	connect(ui.overviewTagsLabel, &QLabel::linkActivated, this, &WikiDialog::overviewLinkActivated);
	connect(ui.pageTagsLabel, &QLabel::linkActivated, this, &WikiDialog::pageTagLinkActivated);
	connect(ui.browseTree, &QTreeWidget::itemDoubleClicked, this, &WikiDialog::browseActivated);
	connect(ui.browseTree, &QTreeWidget::itemSelectionChanged, this, &WikiDialog::browseSelectionChanged);
	connect(ui.browseTree, &QTreeWidget::customContextMenuRequested, this, &WikiDialog::browseContextMenu);
	connect(ui.browseSubscribeButton, &QPushButton::clicked, this, &WikiDialog::browseSubscribe);
	connect(ui.browseOpenButton, &QPushButton::clicked, this, &WikiDialog::browseOpen);
	connect(ui.browseCreateButton, &QPushButton::clicked, this, &WikiDialog::createWiki);

	/* Wiki sidebar */
	connect(ui.backButton, &QToolButton::clicked, this, &WikiDialog::showHome);
	connect(ui.overviewButton, &QToolButton::clicked, this, &WikiDialog::showOverview);
	connect(ui.pageFilterEdit, &QLineEdit::textChanged, this, &WikiDialog::filterPageTree);
	connect(ui.spaceSettingsButton, &QToolButton::clicked, this, &WikiDialog::openSpaceSettings);
	connect(ui.newPageButton, &QToolButton::clicked, this, &WikiDialog::newPage);
	connect(ui.addPageButton, &QToolButton::clicked, this, &WikiDialog::newPage);
	connect(ui.pageTree, &QTreeWidget::itemClicked, this, &WikiDialog::pageTreeClicked);
	connect(ui.pageTree, &QTreeWidget::customContextMenuRequested, this, &WikiDialog::pageTreeContextMenu);

	/* Overview page */
	connect(ui.subscribeButton, &QPushButton::clicked, this, &WikiDialog::toggleSubscribe);
	connect(ui.overviewNewPageButton, &QPushButton::clicked, this, &WikiDialog::newPage);
	connect(ui.overviewPagesTree, &QTreeWidget::itemClicked, this, &WikiDialog::pageListActivated);
	connect(ui.overviewPagesTree, &QTreeWidget::customContextMenuRequested, this, &WikiDialog::overviewPagesContextMenu);

	/* Page */
	connect(ui.breadcrumbLabel, &QLabel::linkActivated, this, &WikiDialog::breadcrumbLinkActivated);
	connect(ui.editButton, &QToolButton::clicked, this, &WikiDialog::editPage);
	connect(ui.historyButton, &QToolButton::clicked, this, &WikiDialog::showPageHistory);
	connect(ui.favoriteButton, &QToolButton::toggled, this, &WikiDialog::favoriteToggled);
	connect(ui.commentsToggleButton, &QToolButton::toggled, this, &WikiDialog::commentsToggled);
	connect(ui.postCommentButton, &QPushButton::clicked, this, &WikiDialog::postComment);

	processSettings(true);

	ui.sidebarStack->setCurrentIndex(0);
	setContentPage(PAGE_HOME);
	browseSelectionChanged();

	/* Get dynamic event type ID for the plugin's wiki events */
	if (rsEvents)
	{
		RsEventType wikiEventType = (RsEventType)rsEvents->getDynamicEventType(RSWIKI_EVENT_TYPE_NAME);
		rsEvents->registerEventsHandler(
			[this](std::shared_ptr<const RsEvent> event)
			{
				RsQThreadUtils::postToObject([=]() { handleEvent_main_thread(event); }, this);
			},
			mEventHandlerId, wikiEventType);
	}
}

WikiDialog::~WikiDialog()
{
	if (rsEvents)
		rsEvents->unregisterEventsHandler(mEventHandlerId);

	processSettings(false);
	delete mEditDialog;
}

UserNotify *WikiDialog::createUserNotify(QObject *parent)
{
	return new WikiUserNotify(rsWiki, this, parent);
}

void WikiDialog::showEvent(QShowEvent *event)
{
	MainPage::showEvent(event);

	/* the stylesheet may have been changed in the settings meanwhile */
	applySkin();

	if (!mInitialLoadDone)
	{
		mInitialLoadDone = true;
		reloadAll();
	}
}

/*********************************************************************************
 * Skin
 *********************************************************************************/

namespace {

/* The beginning of one of RetroShare's built-in stylesheets: RetroShare
 * concatenates the selected sheet into the application stylesheet, so this
 * tells which one is active (also while it is previewed in the settings). */
QString builtInSheetSignature(const QString &name)
{
	QFile file(QString(":/qss/stylesheet/%1.qss").arg(name));
	if (!file.open(QFile::ReadOnly))
		return QString();

	const QString text = QLatin1String(file.readAll());
	return text.trimmed().isEmpty() ? QString() : text.left(200);
}

} // namespace

bool WikiDialog::isLightTheme()
{
	/* dark palette: Windows dark mode or a theme that sets one */
	if (qApp->palette().color(QPalette::Window).lightness() < 128)
		return false;

	static const QString darkSignature = builtInSheetSignature("Standard_Dark");
	static const QString lightSignature = builtInSheetSignature("Standard_Light");

	const QString appSheet = qApp->styleSheet();
	if (!darkSignature.isEmpty() && appSheet.contains(darkSignature))
		return false;
	if (!lightSignature.isEmpty() && appSheet.contains(lightSignature))
		return true;

	/* Built-in style (no stylesheet) is light. A custom stylesheet from the
	 * qss folder can look like anything, so it is left alone; it can style
	 * RsWiki through qss/RsWiki/RsWiki_<Name>.qss. */
	const QString sheet = Settings->getSheetName();
	return sheet.isEmpty() || sheet.startsWith(':');
}

QString WikiDialog::lightSkin()
{
	/* a copy in the config directory overrides the built-in skin */
	QFile file(QString::fromUtf8(RsAccounts::ConfigDirectory().c_str()) + "/qss/RsWiki/RsWiki_light.qss");
	if (!file.exists())
		file.setFileName(":/rswiki/qss/RsWiki_light.qss");

	if (!file.open(QFile::ReadOnly))
		return QString();
	return QString::fromUtf8(file.readAll());
}

void WikiDialog::applySkin()
{
	mApplyingSkin = true;

	QString sheet = mBaseStyleSheet;
	if (isLightTheme())
		sheet += "\n" + lightSkin();

	if (styleSheet() != sheet)
		setStyleSheet(sheet);

	mApplyingSkin = false;
}

void WikiDialog::changeEvent(QEvent *event)
{
	MainPage::changeEvent(event);

	/* RetroShare switched stylesheet or palette: re-check the skin once the
	 * change has settled (not from inside the style change itself) */
	if (!mApplyingSkin && (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange))
		QTimer::singleShot(0, this, &WikiDialog::applySkin);
}

void WikiDialog::processSettings(bool load)
{
	Settings->beginGroup(SETTINGS_GROUP);

	if (load)
	{
		mFavorites = Settings->value("Favorites").toStringList();
		ui.pageSplitter->restoreState(Settings->value("PageSplitter").toByteArray());
		const bool showComments = Settings->value("ShowComments", true).toBool();
		ui.commentsToggleButton->setChecked(showComments);
		ui.commentsPanel->setVisible(showComments);
	}
	else
	{
		Settings->setValue("Favorites", mFavorites);
		Settings->setValue("PageSplitter", ui.pageSplitter->saveState());
		Settings->setValue("ShowComments", ui.commentsToggleButton->isChecked());
	}

	Settings->endGroup();
}

/*********************************************************************************
 * Data
 *********************************************************************************/

void WikiDialog::scheduleReload()
{
	mReloadTimer->start();
}

void WikiDialog::reloadAll()
{
	if (!rsWiki)
		return;

	if (mLoading)
	{
		scheduleReload();
		return;
	}
	mLoading = true;

	/* The service object outlives the plugin's stop(); only the global is
	 * cleared at shutdown, so background work keeps its own copy. */
	RsWiki *wiki = rsWiki;

	RsThread::async([this, wiki]()
	{
		std::vector<RsWikiCollection> collections;
		std::map<RsGxsGroupId, std::vector<RsWikiSnapshot>> pages;
		std::set<RsGxsId> ownIds;
		std::set<RsGxsGroupId> moderated;

		/* returns false when there is no wiki at all */
		wiki->getCollections(std::list<RsGxsGroupId>(), collections);

		std::list<RsGxsId> ownIdList;
		if (rsIdentity && rsIdentity->getOwnIds(ownIdList))
			ownIds.insert(ownIdList.begin(), ownIdList.end());

		for (const RsWikiCollection &c : collections)
		{
			const uint32_t flags = c.mMeta.mSubscribeFlags;
			if (!IS_GROUP_SUBSCRIBED(flags) && !IS_GROUP_ADMIN(flags))
				continue;

			const RsGxsGroupId &groupId = c.mMeta.mGroupId;

			std::vector<RsWikiSnapshot> snapshots;
			std::vector<RsWikiSnapshot> &groupPages = pages[groupId];
			if (wiki->getSnapshots(groupId, snapshots))
			{
				for (RsWikiSnapshot &s : snapshots)
				{
					/* Thread heads only: children are pending edits */
					if (s.mMeta.mParentId.isNull())
						groupPages.push_back(std::move(s));
				}
			}

			if (IS_GROUP_ADMIN(flags))
			{
				moderated.insert(groupId);
				continue;
			}
			for (const RsGxsId &ownId : ownIds)
			{
				if (wiki->isActiveModerator(groupId, ownId, time(nullptr)))
				{
					moderated.insert(groupId);
					break;
				}
			}
		}

		RsQThreadUtils::postToObject(
			[this, collections, pages, ownIds, moderated]()
			{
				mLoading = false;
				mCollections = collections;
				mPages = pages;
				mOwnIds = ownIds;
				mModeratedGroups = moderated;
				refreshAll();
			}, this);
	});
}

RsGxsMessageId WikiDialog::pageOrigId(const RsWikiSnapshot &page)
{
	return page.mMeta.mOrigMsgId.isNull() ? page.mMeta.mMsgId : page.mMeta.mOrigMsgId;
}

const RsWikiCollection *WikiDialog::findCollection(const RsGxsGroupId &groupId) const
{
	for (const RsWikiCollection &c : mCollections)
	{
		if (c.mMeta.mGroupId == groupId)
			return &c;
	}
	return nullptr;
}

const RsWikiSnapshot *WikiDialog::findPage(const RsGxsGroupId &groupId, const RsGxsMessageId &origId) const
{
	if (origId.isNull())
		return nullptr;

	auto it = mPages.find(groupId);
	if (it == mPages.end())
		return nullptr;

	for (const RsWikiSnapshot &page : it->second)
	{
		if (pageOrigId(page) == origId)
			return &page;
	}
	return nullptr;
}

bool WikiDialog::isFollowed(const RsWikiCollection &collection) const
{
	const uint32_t flags = collection.mMeta.mSubscribeFlags;
	return IS_GROUP_SUBSCRIBED(flags) || IS_GROUP_ADMIN(flags);
}

bool WikiDialog::canModerate(const RsGxsGroupId &groupId) const
{
	return mModeratedGroups.count(groupId) > 0;
}

int WikiDialog::unreadCount(const RsGxsGroupId &groupId) const
{
	auto it = mPages.find(groupId);
	if (it == mPages.end())
		return 0;

	return int(std::count_if(it->second.begin(), it->second.end(),
		[](const RsWikiSnapshot &p) { return isUnread(p.mMeta.mMsgStatus); }));
}

QString WikiDialog::authorName(const RsGxsId &id) const
{
	if (id.isNull())
		return QString::fromUtf8("—");

	RsIdentityDetails details;
	if (rsIdentity && rsIdentity->getIdDetails(id, details) && !details.mNickname.empty())
		return toQ(details.mNickname);

	return tr("Unknown");
}

std::vector<const RsWikiCollection *> WikiDialog::followedCollections() const
{
	std::vector<const RsWikiCollection *> list;
	for (const RsWikiCollection &c : mCollections)
	{
		if (isFollowed(c))
			list.push_back(&c);
	}
	std::sort(list.begin(), list.end(), [](const RsWikiCollection *a, const RsWikiCollection *b)
	{
		return toQ(a->mMeta.mGroupName).compare(toQ(b->mMeta.mGroupName), Qt::CaseInsensitive) < 0;
	});
	return list;
}

/*********************************************************************************
 * Favorites
 *********************************************************************************/

QString WikiDialog::favoriteKey(const RsGxsGroupId &groupId, const RsGxsMessageId &origId)
{
	return QString::fromStdString(groupId.toStdString() + ":" + origId.toStdString());
}

bool WikiDialog::isFavorite(const RsGxsGroupId &groupId, const RsGxsMessageId &origId) const
{
	return mFavorites.contains(favoriteKey(groupId, origId));
}

void WikiDialog::setFavorite(const RsGxsGroupId &groupId, const RsGxsMessageId &origId, bool favorite)
{
	const QString key = favoriteKey(groupId, origId);
	mFavorites.removeAll(key);
	if (favorite)
		mFavorites.append(key);

	Settings->beginGroup(SETTINGS_GROUP);
	Settings->setValue("Favorites", mFavorites);
	Settings->endGroup();
}

/*********************************************************************************
 * Refresh
 *********************************************************************************/

void WikiDialog::refreshAll()
{
	fillMyWikis();
	fillSpaceCards();
	fillPageList();
	fillBrowse();

	if (mCurrentGroupId.isNull())
		return;

	if (!findCollection(mCurrentGroupId))
	{
		showHome();
		return;
	}

	fillSpaceSidebar();
	fillPageTree();

	const int page = ui.contentStack->currentIndex();
	if (page == PAGE_OVERVIEW)
	{
		fillOverview();
	}
	else if (page == PAGE_PAGE)
	{
		const RsWikiSnapshot *current = findPage(mCurrentGroupId, mCurrentPageOrigId);
		if (!current)
			showOverview();
		else if (current->mMeta.mMsgId != mCurrentPageMsgId)
			renderPage(*current);   /* a newer version arrived */
		else
			updatePageActions();
	}
}

void WikiDialog::fillMyWikis()
{
	ui.myWikisList->clear();

	for (const RsWikiCollection *c : followedCollections())
	{
		const QString id = QString::fromStdString(c->mMeta.mGroupId.toStdString());
		const QString name = toQ(c->mMeta.mGroupName);
		const int unread = unreadCount(c->mMeta.mGroupId);

		QListWidgetItem *item = new QListWidgetItem(QIcon(wikiAvatar(name, id, 22)),
			unread > 0 ? QString("%1  (%2)").arg(name).arg(unread) : name);
		item->setData(ROLE_GROUP, id);
		if (unread > 0)
		{
			QFont f = item->font();
			f.setBold(true);
			item->setFont(f);
			item->setToolTip(tr("%n unread page(s)", "", unread));
		}
		ui.myWikisList->addItem(item);
	}
}

void WikiDialog::fillSpaceCards()
{
	QLayoutItem *child;
	while ((child = ui.spacesGridLayout->takeAt(0)) != nullptr)
	{
		if (QWidget *w = child->widget())
		{
			w->hide();
			w->deleteLater();
		}
		delete child;
	}

	std::vector<const RsWikiCollection *> followed = followedCollections();
	std::stable_sort(followed.begin(), followed.end(), [](const RsWikiCollection *a, const RsWikiCollection *b)
	{
		return a->mMeta.mLastPost > b->mMeta.mLastPost;
	});

	int n = 0;
	for (const RsWikiCollection *c : followed)
	{
		if (n >= MAX_CARDS)
			break;

		auto it = mPages.find(c->mMeta.mGroupId);
		const int pageCount = (it == mPages.end()) ? 0 : int(it->second.size());

		WikiSpaceCard *card = new WikiSpaceCard(ui.spacesContainer);
		card->setWiki(QString::fromStdString(c->mMeta.mGroupId.toStdString()), toQ(c->mMeta.mGroupName),
		              pageCount, int(c->mMeta.mPop));
		connect(card, &WikiSpaceCard::clicked, this, &WikiDialog::openSpaceFromId);
		ui.spacesGridLayout->addWidget(card, n / CARDS_PER_ROW, n % CARDS_PER_ROW);
		++n;
	}
	for (int col = 0; col < CARDS_PER_ROW; ++col)
		ui.spacesGridLayout->setColumnStretch(col, 1);

	ui.spacesContainer->setVisible(n > 0);
	ui.browseAllButton->setVisible(n > 0);
	ui.noSpacesLabel->setVisible(n == 0);
}

QTreeWidgetItem *WikiDialog::makePageListItem(const RsWikiSnapshot &page) const
{
	const RsGxsGroupId &groupId = page.mMeta.mGroupId;
	const QString groupIdStr = QString::fromStdString(groupId.toStdString());

	SortItem *item = new SortItem;
	item->setIcon(0, QIcon(":/rswiki/images/page.svg"));
	item->setText(0, toQ(page.mMeta.mMsgName));
	setBold(item, 0, isUnread(page.mMeta.mMsgStatus));

	const RsWikiCollection *c = findCollection(groupId);
	item->setText(1, c ? toQ(c->mMeta.mGroupName).toUpper() : QString());
	item->setForeground(1, wikiColor(groupIdStr));
	QFont badge = item->font(1);
	badge.setBold(true);
	if (badge.pointSizeF() > 0)
		badge.setPointSizeF(badge.pointSizeF() * 0.85);
	item->setFont(1, badge);

	const time_t ts = page.mMeta.mPublishTs;
	item->setText(2, friendlyTime(ts));
	item->setData(2, ROLE_SORT, qlonglong(ts));
	item->setToolTip(2, QLocale().toString(QDateTime::fromSecsSinceEpoch(ts), QLocale::LongFormat));

	setItemPage(item, groupId, pageOrigId(page));
	return item;
}

void WikiDialog::fillPageList()
{
	ui.pageListTree->clear();

	const QString search = ui.homeSearchEdit->text().trimmed();

	std::vector<const RsWikiSnapshot *> list;
	for (const auto &groupPages : mPages)
	{
		for (const RsWikiSnapshot &page : groupPages.second)
		{
			if (!search.isEmpty())
			{
				if (!textMatches(search, { toQ(page.mMeta.mMsgName), toQ(page.mPage) }, parseTags(page.mHashTags)))
					continue;
			}
			else if (mListMode == LIST_FAVORITES)
			{
				if (!isFavorite(page.mMeta.mGroupId, pageOrigId(page)))
					continue;
			}
			else if (mListMode == LIST_MINE)
			{
				if (mOwnIds.count(page.mMeta.mAuthorId) == 0)
					continue;
			}
			list.push_back(&page);
		}
	}

	std::sort(list.begin(), list.end(), [](const RsWikiSnapshot *a, const RsWikiSnapshot *b)
	{
		return a->mMeta.mPublishTs > b->mMeta.mPublishTs;
	});
	if (list.size() > size_t(MAX_LIST_ITEMS))
		list.resize(MAX_LIST_ITEMS);

	for (const RsWikiSnapshot *page : list)
		ui.pageListTree->addTopLevelItem(makePageListItem(*page));

	const bool empty = list.empty();
	ui.pageListTree->setVisible(!empty);
	ui.emptyListLabel->setVisible(empty);

	if (!empty)
		return;

	if (!search.isEmpty())
		ui.emptyListLabel->setText(tr("No pages match your search."));
	else if (mListMode == LIST_FAVORITES)
		ui.emptyListLabel->setText(tr("No favorites yet. Use the star on a page to add it here."));
	else if (mListMode == LIST_MINE)
		ui.emptyListLabel->setText(tr("You haven't written any pages yet."));
	else
		ui.emptyListLabel->setText(tr("No pages yet. Open a wiki and create the first page."));
}

void WikiDialog::fillBrowse()
{
	const QString filter = ui.browseFilterEdit->text().trimmed();
	const QString selectedId = ui.browseTree->currentItem()
		? ui.browseTree->currentItem()->data(0, ROLE_GROUP).toString() : QString();

	/* category filter: "All categories" plus the categories in use */
	QString category = ui.browseCategoryCombo->currentData().toString();
	{
		QSignalBlocker blocker(ui.browseCategoryCombo);
		QStringList categories;
		for (const RsWikiCollection &c : mCollections)
		{
			const QString cat = toQ(c.mCategory).trimmed();
			if (!cat.isEmpty() && !categories.contains(cat, Qt::CaseInsensitive))
				categories << cat;
		}
		categories.sort(Qt::CaseInsensitive);

		ui.browseCategoryCombo->clear();
		ui.browseCategoryCombo->addItem(tr("All categories"), QString());
		for (const QString &cat : categories)
			ui.browseCategoryCombo->addItem(cat, cat);

		const int index = ui.browseCategoryCombo->findData(category, Qt::UserRole, Qt::MatchFixedString);
		ui.browseCategoryCombo->setCurrentIndex(index < 0 ? 0 : index);
		category = ui.browseCategoryCombo->currentData().toString();
		ui.browseCategoryCombo->setVisible(!categories.isEmpty());
	}

	ui.browseTree->setSortingEnabled(false);
	ui.browseTree->clear();

	for (const RsWikiCollection &c : mCollections)
	{
		const QString id = QString::fromStdString(c.mMeta.mGroupId.toStdString());
		const QString name = toQ(c.mMeta.mGroupName);
		const QString description = toQ(c.mDescription).simplified();
		const QString wikiCategory = toQ(c.mCategory).trimmed();
		const QStringList wikiTags = parseTags(c.mHashTags);

		if (!category.isEmpty() && wikiCategory.compare(category, Qt::CaseInsensitive) != 0)
			continue;
		if (!filter.isEmpty() && !textMatches(filter, { name, description, wikiCategory }, wikiTags))
			continue;

		SortItem *item = new SortItem;
		item->setIcon(0, QIcon(wikiAvatar(name, id, 24)));
		item->setText(0, name);
		item->setData(0, ROLE_GROUP, id);
		if (!wikiTags.isEmpty())
			item->setToolTip(0, "#" + wikiTags.join("  #"));
		item->setText(1, wikiCategory);
		item->setText(2, description);
		item->setToolTip(2, description);
		item->setText(3, QString::number(c.mMeta.mVisibleMsgCount));
		item->setData(3, ROLE_SORT, qlonglong(c.mMeta.mVisibleMsgCount));
		item->setText(4, QString::number(c.mMeta.mPop));
		item->setData(4, ROLE_SORT, qlonglong(c.mMeta.mPop));
		item->setText(5, friendlyTime(c.mMeta.mLastPost));
		item->setData(5, ROLE_SORT, qlonglong(c.mMeta.mLastPost));

		const uint32_t flags = c.mMeta.mSubscribeFlags;
		if (IS_GROUP_ADMIN(flags))
			item->setText(6, tr("Admin"));
		else if (IS_GROUP_SUBSCRIBED(flags))
			item->setText(6, tr("Subscribed"));
		setBold(item, 0, isFollowed(c));

		ui.browseTree->addTopLevelItem(item);
		if (id == selectedId)
			ui.browseTree->setCurrentItem(item);
	}

	ui.browseTree->setSortingEnabled(true);
	browseSelectionChanged();
}

void WikiDialog::fillSpaceSidebar()
{
	const RsWikiCollection *c = findCollection(mCurrentGroupId);
	if (!c)
		return;

	const QString id = QString::fromStdString(mCurrentGroupId.toStdString());
	const QString name = toQ(c->mMeta.mGroupName);
	ui.spaceSwitchButton->setText(name);
	ui.spaceSwitchButton->setIcon(QIcon(wikiAvatar(name, id, 26)));

	const bool mod = canModerate(mCurrentGroupId);
	const QString noRight = tr("Only the wiki's administrator and moderators can create pages.");
	for (QWidget *w : { static_cast<QWidget*>(ui.newPageButton), static_cast<QWidget*>(ui.addPageButton),
	                    static_cast<QWidget*>(ui.overviewNewPageButton) })
	{
		w->setEnabled(mod);
		w->setToolTip(mod ? QString() : noRight);
	}
	ui.addPageButton->setToolTip(mod ? tr("Add page") : noRight);

	ui.spaceSettingsButton->setText(IS_GROUP_ADMIN(c->mMeta.mSubscribeFlags) ? tr("Wiki settings") : tr("Wiki details"));
}

void WikiDialog::fillPageTree()
{
	/* keep what the user had expanded */
	std::set<RsGxsMessageId> expanded;
	for (QTreeWidgetItemIterator it(ui.pageTree); *it; ++it)
	{
		RsGxsGroupId g;
		RsGxsMessageId o;
		if ((*it)->isExpanded() && itemPage(*it, g, o))
			expanded.insert(o);
	}

	ui.pageTree->clear();

	auto pagesIt = mPages.find(mCurrentGroupId);
	if (pagesIt == mPages.end())
		return;

	std::vector<const RsWikiSnapshot *> pages;
	for (const RsWikiSnapshot &p : pagesIt->second)
		pages.push_back(&p);
	std::sort(pages.begin(), pages.end(), [](const RsWikiSnapshot *a, const RsWikiSnapshot *b)
	{
		return toQ(a->mMeta.mMsgName).compare(toQ(b->mMeta.mMsgName), Qt::CaseInsensitive) < 0;
	});

	std::map<RsGxsMessageId, QTreeWidgetItem *> items;
	std::map<RsGxsMessageId, RsGxsMessageId> parents;
	for (const RsWikiSnapshot *p : pages)
	{
		const RsGxsMessageId origId = pageOrigId(*p);
		QTreeWidgetItem *item = new QTreeWidgetItem;
		item->setText(0, toQ(p->mMeta.mMsgName));
		item->setToolTip(0, toQ(p->mMeta.mMsgName));
		setBold(item, 0, isUnread(p->mMeta.mMsgStatus));
		setItemPage(item, mCurrentGroupId, origId);
		item->setData(0, ROLE_TAGS, parseTags(p->mHashTags));
		items[origId] = item;
		parents[origId] = p->mParentPageId;
	}

	/* a parent chain that loops back makes the page top level */
	auto inCycle = [&parents](const RsGxsMessageId &origId)
	{
		RsGxsMessageId cur = parents[origId];
		for (int depth = 0; depth < 64 && !cur.isNull(); ++depth)
		{
			if (cur == origId)
				return true;
			auto it = parents.find(cur);
			if (it == parents.end())
				return false;
			cur = it->second;
		}
		return false;
	};

	for (const RsWikiSnapshot *p : pages)
	{
		const RsGxsMessageId origId = pageOrigId(*p);
		QTreeWidgetItem *item = items[origId];
		auto parentIt = items.find(p->mParentPageId);

		if (!p->mParentPageId.isNull() && parentIt != items.end() && p->mParentPageId != origId && !inCycle(origId))
			parentIt->second->addChild(item);
		else
			ui.pageTree->addTopLevelItem(item);
	}

	for (auto &entry : items)
	{
		if (expanded.count(entry.first))
			entry.second->setExpanded(true);
	}

	auto currentIt = items.find(mCurrentPageOrigId);
	if (currentIt != items.end())
	{
		for (QTreeWidgetItem *parent = currentIt->second->parent(); parent; parent = parent->parent())
			parent->setExpanded(true);
		ui.pageTree->setCurrentItem(currentIt->second);
	}

	filterPageTree();
}

void WikiDialog::filterPageTree()
{
	const QString filter = ui.pageFilterEdit->text().trimmed();

	std::function<bool(QTreeWidgetItem *)> apply = [&](QTreeWidgetItem *item) -> bool
	{
		bool childMatches = false;
		for (int i = 0; i < item->childCount(); ++i)
			childMatches = apply(item->child(i)) || childMatches;

		const bool matches = filter.isEmpty()
			|| textMatches(filter, { item->text(0) }, item->data(0, ROLE_TAGS).toStringList());
		item->setHidden(!(matches || childMatches));
		if (!filter.isEmpty() && childMatches)
			item->setExpanded(true);
		return matches || childMatches;
	};

	for (int i = 0; i < ui.pageTree->topLevelItemCount(); ++i)
		apply(ui.pageTree->topLevelItem(i));
}

void WikiDialog::fillOverview()
{
	const RsWikiCollection *c = findCollection(mCurrentGroupId);
	if (!c)
		return;

	const QString id = QString::fromStdString(mCurrentGroupId.toStdString());
	const QString name = toQ(c->mMeta.mGroupName);
	const uint32_t flags = c->mMeta.mSubscribeFlags;
	const bool followed = isFollowed(*c);

	ui.overviewAvatarLabel->setPixmap(wikiAvatar(name, id, 64));
	ui.overviewTitleLabel->setText(name);

	const QString description = toQ(c->mDescription).trimmed();
	ui.overviewDescriptionLabel->setText(description.isEmpty() ? tr("No description.") : description);

	auto pagesIt = mPages.find(mCurrentGroupId);
	const int pageCount = followed
		? (pagesIt == mPages.end() ? 0 : int(pagesIt->second.size()))
		: int(c->mMeta.mVisibleMsgCount);

	QString stats = tr("%n page(s)", "", pageCount) + QString::fromUtf8(" · ")
	              + tr("%n subscriber(s)", "", int(c->mMeta.mPop));

	QStringList moderators;
	const rstime_t now = time(nullptr);
	for (const auto &entry : c->mModeratorTerminationDates)
	{
		if (entry.second == 0 || entry.second > now)
			moderators << authorName(entry.first);
	}
	if (!moderators.isEmpty())
		stats += QString::fromUtf8(" · ") + tr("Moderators: %1").arg(moderators.join(", "));
	ui.overviewStatsLabel->setText(stats);

	QStringList extra;
	const QString category = toQ(c->mCategory).trimmed();
	if (!category.isEmpty())
		extra << tr("Category: %1").arg(categoryLink(category));
	const QStringList wikiTags = parseTags(c->mHashTags);
	if (!wikiTags.isEmpty())
		extra << tagLinks(wikiTags, "wikitag");
	ui.overviewTagsLabel->setText(extra.join(QString::fromUtf8(" &nbsp;·&nbsp; ")));
	ui.overviewTagsLabel->setVisible(!extra.isEmpty());

	ui.subscribeButton->setVisible(!IS_GROUP_ADMIN(flags));
	ui.subscribeButton->setText(IS_GROUP_SUBSCRIBED(flags) ? tr("Unsubscribe") : tr("Subscribe"));

	ui.allPagesLabel->setText(followed ? tr("All pages") : tr("Subscribe to this wiki to get its pages."));
	ui.overviewPagesTree->setVisible(followed);

	ui.overviewPagesTree->setSortingEnabled(false);
	ui.overviewPagesTree->clear();
	if (pagesIt != mPages.end())
	{
		for (const RsWikiSnapshot &page : pagesIt->second)
		{
			SortItem *item = new SortItem;
			item->setText(0, toQ(page.mMeta.mMsgName));
			setBold(item, 0, isUnread(page.mMeta.mMsgStatus));
			item->setText(1, authorName(page.mMeta.mAuthorId));
			item->setText(2, friendlyTime(page.mMeta.mPublishTs));
			item->setData(2, ROLE_SORT, qlonglong(page.mMeta.mPublishTs));
			setItemPage(item, mCurrentGroupId, pageOrigId(page));
			ui.overviewPagesTree->addTopLevelItem(item);
		}
	}
	ui.overviewPagesTree->setSortingEnabled(true);
}

/*********************************************************************************
 * Navigation
 *********************************************************************************/

void WikiDialog::setContentPage(ContentPage page)
{
	ui.contentStack->setCurrentIndex(page);
	ui.overviewButton->setChecked(page == PAGE_OVERVIEW);

	if (page == PAGE_HOME)
		(mListMode == LIST_FAVORITES ? ui.navFavoritesButton : ui.navHomeButton)->setChecked(true);
	else if (page == PAGE_BROWSE)
		ui.navBrowseButton->setChecked(true);
}

void WikiDialog::showHome()
{
	mCurrentGroupId.clear();
	mCurrentPageOrigId.clear();
	mCurrentPageMsgId.clear();
	ui.sidebarStack->setCurrentIndex(0);

	mListMode = LIST_RECENT;
	ui.recentTabButton->setChecked(true);
	fillPageList();
	setContentPage(PAGE_HOME);
}

void WikiDialog::showFavorites()
{
	mCurrentGroupId.clear();
	mCurrentPageOrigId.clear();
	ui.sidebarStack->setCurrentIndex(0);

	mListMode = LIST_FAVORITES;
	ui.favoritesTabButton->setChecked(true);
	fillPageList();
	setContentPage(PAGE_HOME);
}

void WikiDialog::showBrowse()
{
	mCurrentGroupId.clear();
	mCurrentPageOrigId.clear();
	ui.sidebarStack->setCurrentIndex(0);

	fillBrowse();
	setContentPage(PAGE_BROWSE);
}

void WikiDialog::listTabChanged()
{
	if (ui.favoritesTabButton->isChecked())
		mListMode = LIST_FAVORITES;
	else if (ui.mineTabButton->isChecked())
		mListMode = LIST_MINE;
	else
		mListMode = LIST_RECENT;

	(mListMode == LIST_FAVORITES ? ui.navFavoritesButton : ui.navHomeButton)->setChecked(true);
	fillPageList();
}

void WikiDialog::homeSearchChanged()
{
	fillPageList();
}

void WikiDialog::openSpaceFromId(const QString &groupId)
{
	openSpace(RsGxsGroupId(groupId.toStdString()), true);
}

void WikiDialog::myWikiClicked(QListWidgetItem *item)
{
	if (item)
		openSpaceFromId(item->data(ROLE_GROUP).toString());
}

void WikiDialog::openSpace(const RsGxsGroupId &groupId, bool showOverviewPage)
{
	if (!findCollection(groupId))
		return;

	if (groupId != mCurrentGroupId)
	{
		mCurrentGroupId = groupId;
		mCurrentPageOrigId.clear();
		mCurrentPageMsgId.clear();
		ui.pageFilterEdit->clear();
	}

	ui.sidebarStack->setCurrentIndex(1);
	fillSpaceSidebar();
	fillPageTree();

	if (showOverviewPage)
		showOverview();
}

void WikiDialog::showOverview()
{
	if (mCurrentGroupId.isNull())
		return;

	mCurrentPageOrigId.clear();
	mCurrentPageMsgId.clear();
	ui.pageTree->clearSelection();

	fillOverview();
	setContentPage(PAGE_OVERVIEW);
}

void WikiDialog::pageTreeClicked(QTreeWidgetItem *item, int /*column*/)
{
	RsGxsGroupId groupId;
	RsGxsMessageId origId;
	if (itemPage(item, groupId, origId))
		openPage(groupId, origId);
}

void WikiDialog::pageListActivated(QTreeWidgetItem *item, int /*column*/)
{
	RsGxsGroupId groupId;
	RsGxsMessageId origId;
	if (itemPage(item, groupId, origId))
		openPage(groupId, origId);
}

void WikiDialog::openPage(const RsGxsGroupId &groupId, const RsGxsMessageId &origId)
{
	const RsWikiSnapshot *page = findPage(groupId, origId);
	if (!page)
		return;

	if (groupId != mCurrentGroupId)
		openSpace(groupId, false);
	else
		ui.sidebarStack->setCurrentIndex(1);

	mCurrentPageOrigId = origId;

	/* the tree may be rebuilt below; keep a copy of the page */
	const RsWikiSnapshot shown = *page;
	renderPage(shown);
	setContentPage(PAGE_PAGE);

	for (QTreeWidgetItemIterator it(ui.pageTree); *it; ++it)
	{
		RsGxsGroupId g;
		RsGxsMessageId o;
		if (itemPage(*it, g, o) && o == origId)
		{
			for (QTreeWidgetItem *parent = (*it)->parent(); parent; parent = parent->parent())
				parent->setExpanded(true);
			ui.pageTree->setCurrentItem(*it);
			break;
		}
	}

	if (isUnread(shown.mMeta.mMsgStatus))
		markPageRead(shown, true);
}

void WikiDialog::renderPage(const RsWikiSnapshot &page)
{
	mCurrentPageMsgId = page.mMeta.mMsgId;

	const QString title = toQ(page.mMeta.mMsgName);
	ui.pageTitleLabel->setText(title);
	/* Pages published without an identity have no author to show */
	const bool hasAuthor = !page.mMeta.mAuthorId.isNull();
	ui.pageMetaPrefixLabel->setVisible(hasAuthor);
	ui.pageAuthorLabel->setVisible(hasAuthor);
	if (hasAuthor)
	{
		ui.pageAuthorLabel->setId(page.mMeta.mAuthorId);
		ui.pageDateLabel->setText(QString::fromUtf8("· ") + friendlyTime(page.mMeta.mPublishTs));
	}
	else
	{
		ui.pageDateLabel->setText(tr("Updated %1").arg(friendlyTime(page.mMeta.mPublishTs)));
	}

	const QStringList pageTags = parseTags(page.mHashTags);
	ui.pageTagsLabel->setText(tagLinks(pageTags, "tag"));
	ui.pageTagsLabel->setVisible(!pageTags.isEmpty());

	/* render Markdown as HTML with cmark (standard line breaks for wiki pages),
	 * then handle links, RetroShare links and embedded images as for posts */
	const QString markdown = toQ(page.mPage);
	ui.textBrowser->setHtml(RsHtml().formatText(ui.textBrowser->document(),
	                                            RsMarkdown::toHtml(markdown, false),
	                                            RSHTML_FORMATTEXT_EMBED_LINKS | RSHTML_FORMATTEXT_MARKDOWN));
	ui.textBrowser->setMarkdownSource(markdown);

	/* Breadcrumb: Wiki › parent pages › this page */
	QStringList crumbs;
	crumbs << QString("<b>%1</b>").arg(title.toHtmlEscaped());

	RsGxsMessageId parentId = page.mParentPageId;
	for (int depth = 0; depth < 16 && !parentId.isNull(); ++depth)
	{
		const RsWikiSnapshot *parent = findPage(page.mMeta.mGroupId, parentId);
		if (!parent || pageOrigId(*parent) == pageOrigId(page))
			break;
		crumbs.prepend(QString("<a href=\"page:%1\" style=\"text-decoration:none\">%2</a>")
			.arg(QString::fromStdString(parentId.toStdString()), toQ(parent->mMeta.mMsgName).toHtmlEscaped()));
		parentId = parent->mParentPageId;
	}

	const RsWikiCollection *c = findCollection(page.mMeta.mGroupId);
	if (c)
		crumbs.prepend(QString("<a href=\"wiki\" style=\"text-decoration:none\">%1</a>")
			.arg(toQ(c->mMeta.mGroupName).toHtmlEscaped()));

	ui.breadcrumbLabel->setText(crumbs.join(QString::fromUtf8(" &nbsp;›&nbsp; ")));

	updatePageActions();
	loadComments();
}

void WikiDialog::breadcrumbLinkActivated(const QString &link)
{
	if (link == "wiki")
		showOverview();
	else if (link.startsWith("page:"))
		openPage(mCurrentGroupId, RsGxsMessageId(link.mid(5).toStdString()));
}

void WikiDialog::pageTagLinkActivated(const QString &link)
{
	/* all pages with this tag, in every wiki */
	showHome();
	ui.homeSearchEdit->setText("#" + tagFromLink(link));
}

void WikiDialog::overviewLinkActivated(const QString &link)
{
	if (link.startsWith("category:"))
		showBrowseFor(tagFromLink(link), QString());
	else if (link.startsWith("wikitag:"))
		showBrowseFor(QString(), "#" + tagFromLink(link));
}

void WikiDialog::showBrowseFor(const QString &category, const QString &filter)
{
	showBrowse();
	{
		QSignalBlocker blockEdit(ui.browseFilterEdit);
		QSignalBlocker blockCombo(ui.browseCategoryCombo);
		ui.browseFilterEdit->setText(filter);
		const int index = ui.browseCategoryCombo->findData(category, Qt::UserRole, Qt::MatchFixedString);
		ui.browseCategoryCombo->setCurrentIndex(index < 0 ? 0 : index);
	}
	fillBrowse();
}

void WikiDialog::noSpacesLinkActivated(const QString &link)
{
	if (link == "create")
		createWiki();
	else
		showBrowse();
}

void WikiDialog::updatePageActions()
{
	const RsWikiCollection *c = findCollection(mCurrentGroupId);
	const bool followed = c && isFollowed(*c);

	ui.editButton->setEnabled(followed);
	ui.historyButton->setEnabled(followed);

	{
		QSignalBlocker blocker(ui.favoriteButton);
		const bool favorite = isFavorite(mCurrentGroupId, mCurrentPageOrigId);
		ui.favoriteButton->setChecked(favorite);
		ui.favoriteButton->setToolTip(favorite ? tr("Remove from favorites") : tr("Add to favorites"));
	}

	ui.composerFrame->setEnabled(followed);
	ui.commentEdit->setPlaceholderText(followed ? tr("Write a comment…") : tr("Subscribe to this wiki to comment."));
}

/*********************************************************************************
 * Wikis
 *********************************************************************************/

void WikiDialog::createWiki()
{
	WikiGroupDialog dialog(this);
	dialog.exec();
	scheduleReload();
}

void WikiDialog::openSpaceSettings()
{
	const RsWikiCollection *c = findCollection(mCurrentGroupId);
	if (!c)
		return;

	const GxsGroupDialog::Mode mode = IS_GROUP_ADMIN(c->mMeta.mSubscribeFlags)
		? GxsGroupDialog::MODE_EDIT : GxsGroupDialog::MODE_SHOW;
	WikiGroupDialog dialog(mode, mCurrentGroupId, this);
	dialog.exec();
	scheduleReload();
}

void WikiDialog::toggleSubscribe()
{
	const RsWikiCollection *c = findCollection(mCurrentGroupId);
	if (!c || !rsWiki)
		return;

	uint32_t token;
	rsWiki->subscribeToGroup(token, mCurrentGroupId, !IS_GROUP_SUBSCRIBED(c->mMeta.mSubscribeFlags));
	scheduleReload();
}

void WikiDialog::browseFilterChanged()
{
	fillBrowse();
}

void WikiDialog::browseSelectionChanged()
{
	QTreeWidgetItem *item = ui.browseTree->currentItem();
	const RsWikiCollection *c = item
		? findCollection(RsGxsGroupId(item->data(0, ROLE_GROUP).toString().toStdString())) : nullptr;

	ui.browseOpenButton->setEnabled(c != nullptr);
	ui.browseSubscribeButton->setEnabled(c && !IS_GROUP_ADMIN(c->mMeta.mSubscribeFlags));
	ui.browseSubscribeButton->setText(c && IS_GROUP_SUBSCRIBED(c->mMeta.mSubscribeFlags)
		? tr("Unsubscribe") : tr("Subscribe"));
}

void WikiDialog::browseSubscribe()
{
	QTreeWidgetItem *item = ui.browseTree->currentItem();
	if (!item || !rsWiki)
		return;

	const RsGxsGroupId groupId(item->data(0, ROLE_GROUP).toString().toStdString());
	const RsWikiCollection *c = findCollection(groupId);
	if (!c || IS_GROUP_ADMIN(c->mMeta.mSubscribeFlags))
		return;

	uint32_t token;
	rsWiki->subscribeToGroup(token, groupId, !IS_GROUP_SUBSCRIBED(c->mMeta.mSubscribeFlags));
	scheduleReload();
}

void WikiDialog::browseOpen()
{
	if (QTreeWidgetItem *item = ui.browseTree->currentItem())
		openSpaceFromId(item->data(0, ROLE_GROUP).toString());
}

void WikiDialog::browseActivated(QTreeWidgetItem *item, int /*column*/)
{
	if (item)
		openSpaceFromId(item->data(0, ROLE_GROUP).toString());
}

/*********************************************************************************
 * Pages
 *********************************************************************************/

void WikiDialog::newPage()
{
	startNewPage(RsGxsMessageId());
}

void WikiDialog::newSubPage()
{
	startNewPage(mCurrentPageOrigId);
}

void WikiDialog::startNewPage(const RsGxsMessageId &parentOrigId)
{
	if (mCurrentGroupId.isNull())
		return;

	if (!canModerate(mCurrentGroupId))
	{
		QMessageBox::information(this, tr("New page"),
			tr("Only the wiki's administrator and moderators can create pages."));
		return;
	}

	if (!mEditDialog)
		mEditDialog = new WikiEditDialog(nullptr);

	mEditDialog->setupData(mCurrentGroupId, RsGxsMessageId());
	mEditDialog->setNewPage();
	mEditDialog->setParentPage(parentOrigId);
	mEditDialog->show();
	mEditDialog->raise();
	mEditDialog->activateWindow();
}

void WikiDialog::openEditor(bool history)
{
	const RsWikiSnapshot *page = findPage(mCurrentGroupId, mCurrentPageOrigId);
	if (!page)
		return;

	if (!mEditDialog)
		mEditDialog = new WikiEditDialog(nullptr);

	mEditDialog->setupData(mCurrentGroupId, page->mMeta.mMsgId);
	if (history)
		mEditDialog->showHistory();
	mEditDialog->show();
	mEditDialog->raise();
	mEditDialog->activateWindow();
}

void WikiDialog::editPage()
{
	openEditor(false);
}

void WikiDialog::showPageHistory()
{
	openEditor(true);
}

void WikiDialog::republishPage()
{
	if (!canModerate(mCurrentGroupId))
		return;

	openEditor(false);
	if (mEditDialog && !mCurrentPageOrigId.isNull())
	{
		RsGxsMessageId origId = mCurrentPageOrigId;
		mEditDialog->setRepublishMode(origId);
	}
}

void WikiDialog::favoriteToggled(bool checked)
{
	if (mCurrentPageOrigId.isNull())
		return;

	setFavorite(mCurrentGroupId, mCurrentPageOrigId, checked);
	ui.favoriteButton->setToolTip(checked ? tr("Remove from favorites") : tr("Add to favorites"));
	fillPageList();
}

void WikiDialog::commentsToggled(bool checked)
{
	ui.commentsPanel->setVisible(checked);
	ui.commentsToggleButton->setToolTip(checked ? tr("Hide comments") : tr("Show comments"));
}

void WikiDialog::markCurrentPageUnread()
{
	if (const RsWikiSnapshot *page = findPage(mCurrentGroupId, mCurrentPageOrigId))
		markPageRead(*page, false);
}

void WikiDialog::markPageRead(const RsWikiSnapshot &page, bool read)
{
	if (!rsWiki)
		return;

	const RsGxsGrpMsgIdPair msgId(page.mMeta.mGroupId, page.mMeta.mMsgId);
	RsWiki *wiki = rsWiki;
	RsThread::async([wiki, msgId, read]()
	{
		wiki->setMessageReadStatus(msgId, read);
	});
}

/*********************************************************************************
 * Comments
 *********************************************************************************/

void WikiDialog::clearComments()
{
	const QList<WikiCommentItem *> items =
		ui.commentsContainer->findChildren<WikiCommentItem *>(QString(), Qt::FindDirectChildrenOnly);
	for (WikiCommentItem *item : items)
	{
		ui.commentsLayout->removeWidget(item);
		item->hide();
		item->deleteLater();
	}
	ui.noCommentsLabel->setVisible(true);
	ui.commentsHeaderLabel->setText(tr("Comments"));
}

void WikiDialog::loadComments()
{
	clearComments();
	mCommentsLoadedFor = mCurrentPageOrigId;

	if (mCurrentGroupId.isNull() || mCurrentPageOrigId.isNull() || !rsWiki)
		return;

	const RsGxsGroupId groupId = mCurrentGroupId;
	const RsGxsMessageId threadId = mCurrentPageOrigId;

	RsWiki *wiki = rsWiki;
	RsThread::async([this, wiki, groupId, threadId]()
	{
		std::vector<RsWikiComment> comments;
		if (!wiki->getPageComments(groupId, threadId, comments))
			comments.clear();

		RsQThreadUtils::postToObject([this, groupId, threadId, comments]()
		{
			if (groupId != mCurrentGroupId || threadId != mCurrentPageOrigId)
				return;   /* the user moved on */

			clearComments();
			for (const RsWikiComment &comment : comments)
			{
				/* insert before the trailing spacer */
				ui.commentsLayout->insertWidget(ui.commentsLayout->count() - 1,
				                                new WikiCommentItem(comment, ui.commentsContainer));
			}
			ui.noCommentsLabel->setVisible(comments.empty());
			if (!comments.empty())
				ui.commentsHeaderLabel->setText(tr("Comments (%1)").arg(comments.size()));
		}, this);
	});
}

void WikiDialog::postComment()
{
	const QString text = ui.commentEdit->toPlainText().trimmed();
	if (text.isEmpty() || !rsWiki)
		return;

	const RsWikiSnapshot *page = findPage(mCurrentGroupId, mCurrentPageOrigId);
	if (!page)
		return;

	RsGxsId authorId;
	switch (ui.commentIdChooser->getChosenId(authorId))
	{
	case GxsIdChooser::KnowId:
	case GxsIdChooser::UnKnowId:
		break;
	default:
		QMessageBox::warning(this, tr("Post comment"), tr("Please choose an identity to post with."));
		return;
	}

	RsWikiComment comment;
	comment.mMeta.mGroupId = mCurrentGroupId;
	comment.mMeta.mAuthorId = authorId;
	comment.mMeta.mThreadId = mCurrentPageOrigId;      /* the page */
	comment.mMeta.mParentId = page->mMeta.mMsgId;      /* the version commented on */
	comment.mMeta.mMsgName = page->mMeta.mMsgName;
	comment.mComment = text.toUtf8().constData();

	ui.postCommentButton->setEnabled(false);

	RsWiki *wiki = rsWiki;
	RsThread::async([this, wiki, comment]() mutable
	{
		const bool ok = wiki->createComment(comment);

		RsQThreadUtils::postToObject([this, ok]()
		{
			ui.postCommentButton->setEnabled(true);
			if (ok)
			{
				ui.commentEdit->clear();
				loadComments();
			}
			else
			{
				QMessageBox::warning(this, tr("Post comment"), tr("The comment could not be published."));
			}
		}, this);
	});
}

/*********************************************************************************
 * Context menus
 *********************************************************************************/

void WikiDialog::setItemPage(QTreeWidgetItem *item, const RsGxsGroupId &groupId, const RsGxsMessageId &origId)
{
	item->setData(0, ROLE_GROUP, QString::fromStdString(groupId.toStdString()));
	item->setData(0, ROLE_ORIG, QString::fromStdString(origId.toStdString()));
}

bool WikiDialog::itemPage(const QTreeWidgetItem *item, RsGxsGroupId &groupId, RsGxsMessageId &origId)
{
	if (!item)
		return false;

	const QString group = item->data(0, ROLE_GROUP).toString();
	const QString orig = item->data(0, ROLE_ORIG).toString();
	if (group.isEmpty() || orig.isEmpty())
		return false;

	groupId = RsGxsGroupId(group.toStdString());
	origId = RsGxsMessageId(orig.toStdString());
	return !groupId.isNull() && !origId.isNull();
}

void WikiDialog::showPageContextMenu(const RsGxsGroupId &groupId, const RsGxsMessageId &origId, bool inCurrentWiki)
{
	const RsWikiSnapshot *page = findPage(groupId, origId);
	if (!page)
		return;

	const RsWikiSnapshot copy = *page;
	QMenu menu(this);

	menu.addAction(tr("Open"), this, [this, groupId, origId]() { openPage(groupId, origId); });

	if (inCurrentWiki && canModerate(groupId))
	{
		menu.addAction(QIcon(":/rswiki/images/plus.svg"), tr("Add sub-page"), this,
			[this, origId]() { startNewPage(origId); });
	}

	menu.addSeparator();

	const bool favorite = isFavorite(groupId, origId);
	menu.addAction(favorite ? tr("Remove from favorites") : tr("Add to favorites"), this,
		[this, groupId, origId, favorite]()
		{
			setFavorite(groupId, origId, !favorite);
			fillPageList();
			if (groupId == mCurrentGroupId && origId == mCurrentPageOrigId)
				updatePageActions();
		});

	const bool unread = isUnread(copy.mMeta.mMsgStatus);
	menu.addAction(unread ? tr("Mark as read") : tr("Mark as unread"), this,
		[this, copy, unread]() { markPageRead(copy, unread); });

	menu.exec(QCursor::pos());
}

void WikiDialog::pageTreeContextMenu(const QPoint &point)
{
	RsGxsGroupId groupId;
	RsGxsMessageId origId;
	if (itemPage(ui.pageTree->itemAt(point), groupId, origId))
	{
		showPageContextMenu(groupId, origId, true);
		return;
	}

	if (!canModerate(mCurrentGroupId))
		return;

	QMenu menu(this);
	menu.addAction(QIcon(":/rswiki/images/plus.svg"), tr("New page"), this, SLOT(newPage()));
	menu.exec(QCursor::pos());
}

void WikiDialog::pageListContextMenu(const QPoint &point)
{
	RsGxsGroupId groupId;
	RsGxsMessageId origId;
	if (itemPage(ui.pageListTree->itemAt(point), groupId, origId))
		showPageContextMenu(groupId, origId, false);
}

void WikiDialog::overviewPagesContextMenu(const QPoint &point)
{
	RsGxsGroupId groupId;
	RsGxsMessageId origId;
	if (itemPage(ui.overviewPagesTree->itemAt(point), groupId, origId))
		showPageContextMenu(groupId, origId, true);
}

void WikiDialog::browseContextMenu(const QPoint &point)
{
	QTreeWidgetItem *item = ui.browseTree->itemAt(point);
	if (!item)
		return;
	ui.browseTree->setCurrentItem(item);

	const RsGxsGroupId groupId(item->data(0, ROLE_GROUP).toString().toStdString());
	const RsWikiCollection *c = findCollection(groupId);
	if (!c)
		return;

	QMenu menu(this);
	menu.addAction(tr("Open"), this, SLOT(browseOpen()));
	if (!IS_GROUP_ADMIN(c->mMeta.mSubscribeFlags))
	{
		menu.addAction(IS_GROUP_SUBSCRIBED(c->mMeta.mSubscribeFlags) ? tr("Unsubscribe") : tr("Subscribe"),
		               this, SLOT(browseSubscribe()));
	}
	menu.addSeparator();
	menu.addAction(QIcon(":/rswiki/images/info.svg"), tr("Details"), this, [this, groupId]()
	{
		WikiGroupDialog dialog(GxsGroupDialog::MODE_SHOW, groupId, this);
		dialog.exec();
	});
	menu.exec(QCursor::pos());
}

/*********************************************************************************
 * Events
 *********************************************************************************/

void WikiDialog::handleEvent_main_thread(std::shared_ptr<const RsEvent> event)
{
	const RsGxsWikiEvent *e = dynamic_cast<const RsGxsWikiEvent *>(event.get());
	if (!e)
		return;

	if (e->mWikiEventCode == RsWikiEventCode::NEW_COMMENT)
	{
		if (e->mWikiGroupId == mCurrentGroupId && ui.contentStack->currentIndex() == PAGE_PAGE)
			loadComments();
		return;
	}

	scheduleReload();
}

} // namespace rswiki
