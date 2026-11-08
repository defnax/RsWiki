/*******************************************************************************
 * plugins/RsWiki/RsWikiPlugin.h                                               *
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

#ifndef RSWIKI_PLUGIN_H
#define RSWIKI_PLUGIN_H

#include <retroshare/rsplugin.h>

#include <string>

namespace rswiki {
class p3Wiki;
class WikiDialog;
}
class RsGxsNetService;
class RsGeneralDataService;
class RsNxsNetMgr;
class RsGixs;
class RsReputations;
class RsGcxs;
class RsGxsIdExchange;
class PgpAuxUtils;

/*
 * RsWiki plugin
 *
 * A GXS wiki service (derived from WikiPoos) as a loadable plugin. The
 * plugin owns its data store, the p3Wiki exchange service and its
 * RsGxsNetService, and provides the wiki page for the main window.
 *
 * It is independent of the in-core WikiPoos service (CONFIG += wikipoos /
 * RS_USE_WIKI): own service id, database, config file and event type, and
 * all its code lives in namespace rswiki, so both can be enabled together.
 */
class RsWikiPlugin : public RsPlugin
{
public:
	RsWikiPlugin();
	virtual ~RsWikiPlugin();

	virtual void setInterfaces(RsPlugInInterfaces& interfaces) override;
	virtual void setPlugInHandler(RsPluginHandler *pgHandler) override;
	virtual void getPluginVersion(int& major, int& minor, int& build, int& svn_rev) const override;

	virtual p3Service *p3_service() const override;
	virtual p3Config  *p3_config()  const override;
	virtual std::string configurationFileName() const override { return "rswiki.cfg"; }
	virtual uint16_t rs_service_id() const override;

	virtual void stop() override;

	virtual QIcon      *qt_icon() const override;
	virtual MainPage   *qt_page() const override;
	virtual QDialog    *qt_about_page() const override;
	virtual QTranslator *qt_translator(QApplication *app, const QString& languageCode, const QString& externalDir) const override;

	/* RetroShare loads qss/RsWiki/RsWiki_<sheet>.qss with the active stylesheet */
	virtual std::string qt_stylesheet() override { return "RsWiki"; }

	virtual std::string getShortPluginDescription() const override;
	virtual std::string getPluginName() const override;

private:
	/* Creates the data store, p3Wiki and its network service on first use */
	void createService() const;

	mutable RsGeneralDataService *mDataStore;
	mutable rswiki::p3Wiki *mWiki;
	mutable RsGxsNetService *mWikiNetService;

	mutable rswiki::WikiDialog *mMainPage;
	mutable QIcon *mIcon;

	RsPluginHandler *mPlugInHandler;

	RsNxsNetMgr *mNxsMgr;
	RsGixs *mGixs;
	RsReputations *mReputations;
	RsGcxs *mGxsCircles;
	RsGxsIdExchange *mGxsIdService;
	PgpAuxUtils *mPgpAuxUtils;
	std::string mGxsDir;
};

#endif // RSWIKI_PLUGIN_H
