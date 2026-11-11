/*******************************************************************************
 * plugins/RsWiki/services: p3RsWiki.h                                         *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright 2012-2012 by Robert Fernie <retroshare@lunamutt.com>              *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Lesser General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Lesser General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Lesser General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/
#ifndef RSWIKI_PLUGIN_P3WIKI_H
#define RSWIKI_PLUGIN_P3WIKI_H

#include "interface/rsWiki.h"
#include "gxs/rsgenexchange.h"

#include "util/rstickevent.h"

#include <map>
#include <string>

namespace rswiki {

/* 
 * Wiki Service
 *
 *
 */

class p3Wiki: public RsGenExchange, public RsWiki
{
public:
	p3Wiki(RsGeneralDataService* gds, RsNetworkExchangeService* nes, RsGixs *gixs);
	virtual RsServiceInfo getServiceInfo() override;
	static uint32_t wikiAuthenPolicy();
    
	/* Required by base class */
	virtual void service_tick() override;

protected:
	/* Triggered on GXS updates */
	virtual void notifyChanges(std::vector<RsGxsNotify*>& changes) override;

public:
	/* GXS Data Access Methods */
	virtual bool getCollections(const uint32_t &token, std::vector<RsWikiCollection> &collections) override;
	virtual bool getSnapshots(const uint32_t &token, std::vector<RsWikiSnapshot> &snapshots) override;
	virtual bool getComments(const uint32_t &token, std::vector<RsWikiComment> &comments) override;
	virtual bool getRelatedSnapshots(const uint32_t &token, std::vector<RsWikiSnapshot> &snapshots) override;
    
	virtual bool submitCollection(uint32_t &token, RsWikiCollection &collection) override;
	virtual bool submitSnapshot(uint32_t &token, RsWikiSnapshot &snapshot) override;
	virtual bool submitComment(uint32_t &token, RsWikiComment &comment) override;
	virtual bool updateCollection(uint32_t &token, RsWikiCollection &collection) override;

	/* Blocking Interfaces */
	virtual bool createCollection(RsWikiCollection &collection) override;
	virtual bool updateCollection(const RsWikiCollection &collection) override;
	virtual bool getCollections(const std::list<RsGxsGroupId> groupIds, std::vector<RsWikiCollection> &groups) override;
	virtual bool getSnapshot(const RsGxsGrpMsgIdPair& msgId, RsWikiSnapshot& snapshot) override;
	virtual bool getSnapshots(const RsGxsGroupId& groupId, std::vector<RsWikiSnapshot>& snapshots) override;
	virtual bool getRelatedSnapshots(const RsGxsGrpMsgIdPair& msgId, std::vector<RsWikiSnapshot>& snapshots) override;
	virtual bool getPageEdits(const RsGxsGroupId& grpId, const RsGxsMessageId& pageThreadId,
	                          std::vector<RsWikiSnapshot>& edits) override;
	virtual bool setMessageReadStatus(const RsGxsGrpMsgIdPair& msgId, bool read) override;

	/* Moderator management */
	virtual bool addModerator(const RsGxsGroupId& grpId, const RsGxsId& moderatorId) override;
	virtual bool removeModerator(const RsGxsGroupId& grpId, const RsGxsId& moderatorId) override;
	virtual bool getModerators(const RsGxsGroupId& grpId, std::list<RsGxsId>& moderators) override;
	virtual bool isActiveModerator(const RsGxsGroupId& grpId, const RsGxsId& authorId, rstime_t editTime) override;

	/* Content fetching for merge operations (Todo 3) */
	virtual bool getSnapshotContent(const RsGxsGroupId& grpId,
	                                const RsGxsMessageId& snapshotId,
	                                std::string& content) override;
	virtual bool getSnapshotsContent(const RsGxsGroupId& grpId,
	                                 const std::vector<RsGxsMessageId>& snapshotIds,
	                                 std::map<RsGxsMessageId, std::string>& contents) override;

	/* Notification support */
	virtual bool getWikiStatistics(GxsServiceStatistic& stats) override;
	virtual void setMessageReadStatus(uint32_t& token, const RsGxsGrpMsgIdPair& msgId, bool read) override;

	/* Comments */
	virtual bool getPageComments(const RsGxsGroupId& grpId, const RsGxsMessageId& pageThreadId,
	                             std::vector<RsWikiComment>& comments) override;
	virtual bool createComment(RsWikiComment& comment) override;

	/* Read status of whole pages */
	virtual bool getUnreadChildCounts(const RsGxsGroupId& grpId,
	                                  std::map<RsGxsMessageId, uint32_t>& unreadChildren) override;
	virtual bool markPageThreadRead(const RsGxsGroupId& grpId, const RsGxsMessageId& pageThreadId) override;
	virtual bool markWikiRead(const RsGxsGroupId& grpId) override;

protected:
	bool acceptNewMessage(const RsGxsMsgMetaData *msgMeta, uint32_t size) override;

private:
	bool checkModeratorPermission(const RsGxsGroupId& grpId, const RsGxsId& authorId, const RsGxsId& originalAuthorId, rstime_t editTime);
	bool getCollectionData(const RsGxsGroupId& grpId, RsWikiCollection& collection);
	bool getOriginalMessageAuthor(const RsGxsGroupId& grpId, const RsGxsMessageId& msgId, RsGxsId& authorId);
	bool getGroupMsgMeta(const RsGxsGroupId& grpId, std::vector<RsMsgMetaData>& metas);
	/* marks the given messages read and sends one READ_STATUS_CHANGED event */
	bool markMessagesRead(const RsGxsGroupId& grpId, const std::vector<RsGxsMessageId>& msgIds);
	
	// Track known wikis to distinguish NEW from UPDATED
	std::map<RsGxsGroupId, rstime_t> mKnownWikis;
	RsMutex mKnownWikisMutex;
};

} // namespace rswiki

#endif 
