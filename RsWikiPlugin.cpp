/*******************************************************************************
 * plugins/RsWiki/RsWikiPlugin.cpp                                             *
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

#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QString>
#include <QTranslator>

#include <cstdlib>

#include "RsWikiPlugin.h"
#include "services/p3RsWiki.h"
#include "gui/WikiDialog.h"

#include <gxs/rsdataservice.h>
#include <gxs/rsgxsnetservice.h>
#include <util/rsdir.h>

#include <retroshare/rsinit.h>
#include <retroshare/rsreputations.h>
#include <retroshare/rsversion.h>

#define IMAGE_WIKI ":/icons/png/wiki.png"   /* shipped with retroshare-gui */

/* Own service id and database, separate from the in-core WikiPoos service
 * (RsServiceType::WIKI, "wiki_db", "wiki.cfg"), so both can run together. */
static const uint16_t RSWIKI_SERVICE_ID = rswiki::RS_SERVICE_TYPE_RSWIKI_PLUGIN;
static const char RSWIKI_DB_NAME[] = "rswiki_db";

/* Keeps this library loaded until the process exits (defined at the end) */
static void pinPluginLibrary();

/* Q_INIT_RESOURCE must be used outside of any namespace */
static void initRsWikiResources()
{
	Q_INIT_RESOURCE(RsWiki_images);
}

extern "C" {

	// This is *the* function required by RS plugin system to give RS access to the plugin.
	// Be careful to:
	// - always respect the C linkage convention
	// - always return an object of type RsPlugin*
	//
	void *RETROSHARE_PLUGIN_provide()
	{
		static RsWikiPlugin *p = new RsWikiPlugin();

		return (void*)p;
	}

	// This symbol contains the svn revision number grabbed from the executable.
	// It will be tested by RS to load the plugin automatically, since it is safe to load plugins
	// with same revision numbers, assuming that the revision numbers are up-to-date.
	//
	uint32_t RETROSHARE_PLUGIN_revision = abs(atoi(RS_EXTRA_VERSION));

	// This symbol contains the plugin API version the plugin was built against.
	//
	uint32_t RETROSHARE_PLUGIN_api = RS_PLUGIN_API_VERSION;
}

RsWikiPlugin::RsWikiPlugin()
	: mDataStore(nullptr)
	, mWiki(nullptr)
	, mWikiNetService(nullptr)
	, mMainPage(nullptr)
	, mIcon(nullptr)
	, mPlugInHandler(nullptr)
	, mNxsMgr(nullptr)
	, mGixs(nullptr)
	, mReputations(nullptr)
	, mGxsCircles(nullptr)
	, mGxsIdService(nullptr)
	, mPgpAuxUtils(nullptr)
{
	/* RetroShare unloads plugin libraries in stopPlugins(), right after
	 * stop(). Objects whose code is in this library outlive that call: the
	 * p3Wiki service (the core's net service still points to it), wiki
	 * events still queued in rsEvents, and background loads. Unloading the
	 * code under them crashes the shutdown, so the library stays mapped. */
	pinPluginLibrary();
}

RsWikiPlugin::~RsWikiPlugin()
{
}

void RsWikiPlugin::getPluginVersion(int& major, int& minor, int& build, int& svn_rev) const
{
	major = RS_MAJOR_VERSION;
	minor = RS_MINOR_VERSION;
	build = RS_MINI_VERSION;
	svn_rev = abs(atoi(RS_EXTRA_VERSION));
}

uint16_t RsWikiPlugin::rs_service_id() const
{
	return RSWIKI_SERVICE_ID;
}

void RsWikiPlugin::setInterfaces(RsPlugInInterfaces &interfaces)
{
	mNxsMgr       = interfaces.mRsNxsNetMgr;
	mGixs         = interfaces.mGxsIdService;
	mReputations  = interfaces.mReputations;
	mGxsCircles   = interfaces.mGxsCirlces;
	mGxsIdService = interfaces.mGxsIdService;
	mPgpAuxUtils  = interfaces.mPgpAuxUtils;
	mGxsDir       = interfaces.mGxsDir;
}

void RsWikiPlugin::setPlugInHandler(RsPluginHandler *pgHandler)
{
	mPlugInHandler = pgHandler;
}

void RsWikiPlugin::createService() const
{
	if (mWiki)
		return;

	std::string gxsDir = mGxsDir;
	if (gxsDir.empty())
		gxsDir = RsAccounts::AccountDirectory() + "/gxs";
	RsDirUtil::checkCreateDirectory(gxsDir);

	// The core opened its GXS databases with the login password; plugins do
	// not get it, so (like RetroGit) the store is opened with an empty one.
	mDataStore = new RsDataService(gxsDir + "/", RSWIKI_DB_NAME, RSWIKI_SERVICE_ID, nullptr, "");

	mWiki = new rswiki::p3Wiki(mDataStore, nullptr, mGixs);

	mWikiNetService = new RsGxsNetService(
	            RSWIKI_SERVICE_ID, mDataStore, mNxsMgr,
	            mWiki, mWiki->getServiceInfo(),
	            dynamic_cast<RsGixsReputation*>(mReputations), mGxsCircles, mGxsIdService,
	            mPgpAuxUtils);

	mWiki->setNetworkExchangeService(mWikiNetService);

	// The plugin GUI reaches the service through this (namespaced) global.
	rswiki::rsWiki = mWiki;

	mWiki->start("gxs wiki");
	mWikiNetService->start("gxs wiki ns");
}

p3Service *RsWikiPlugin::p3_service() const
{
	createService();
	return mWikiNetService;
}

p3Config *RsWikiPlugin::p3_config() const
{
	// The network service keeps the sync state, saved in wiki.cfg
	createService();
	return mWikiNetService;
}

void RsWikiPlugin::stop()
{
	// Stop the network service before the exchange service it feeds.
	if (mWikiNetService)
		mWikiNetService->fullstop();

	if (mWiki)
		mWiki->fullstop();

	rswiki::rsWiki = nullptr;
}

QIcon *RsWikiPlugin::qt_icon() const
{
	if (!mIcon)
	{
		initRsWikiResources();
		mIcon = new QIcon(IMAGE_WIKI);
	}

	return mIcon;
}

MainPage *RsWikiPlugin::qt_page() const
{
	if (!mMainPage)
	{
		initRsWikiResources();
		mMainPage = new rswiki::WikiDialog();
	}

	return mMainPage;
}

QDialog *RsWikiPlugin::qt_about_page() const
{
	static QMessageBox *about_dialog = nullptr;

	if (!about_dialog)
	{
		about_dialog = new QMessageBox();

		QString text;
		text += QObject::tr("<h3>RetroShare RsWiki plugin</h3><br/>");
		text += QObject::tr("Decentralized wikis shared over GXS: create wiki groups, "
		                    "write and edit pages, and appoint moderators.");

		about_dialog->setWindowTitle(QObject::tr("About RsWiki"));
		about_dialog->setText(text);
		about_dialog->setStandardButtons(QMessageBox::Ok);
	}

	return about_dialog;
}

QTranslator *RsWikiPlugin::qt_translator(QApplication * /*app*/, const QString& languageCode, const QString& externalDir) const
{
	if (languageCode == "en")
		return nullptr;

	QTranslator *translator = new QTranslator();
	if (translator->load(externalDir + "/RsWiki_" + languageCode + ".qm"))
		return translator;
	if (translator->load(":/lang/RsWiki_" + languageCode + ".qm"))
		return translator;

	delete translator;
	return nullptr;
}

std::string RsWikiPlugin::getShortPluginDescription() const
{
	return QObject::tr("Decentralized wiki collections shared over GXS.").toUtf8().constData();
}

std::string RsWikiPlugin::getPluginName() const
{
	return "RsWiki";
}

/*********************************************************************************
 * Pin the plugin library
 *********************************************************************************/

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

static void pinPluginLibrary()
{
	/* Takes a reference on this DLL that FreeLibrary() never releases */
	HMODULE self = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
	                   reinterpret_cast<LPCWSTR>(&pinPluginLibrary), &self);
}
#else
static void pinPluginLibrary()
{
	/* ELF: linked with -z nodelete (see RsWiki.pro / CMakeLists.txt) */
}
#endif
