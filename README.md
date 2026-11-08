# RsWiki – RetroShare wiki plugin

A GXS wiki service, derived from the in-core **WikiPoos** service
(`CONFIG += wikipoos` / `RS_GXSWIKIPOS` / `RS_USE_WIKI`), packaged as a
loadable RetroShare plugin.

The plugin owns the whole service:

- its GXS data store (`gxs/rswiki_db`),
- the `p3Wiki` exchange service and its `RsGxsNetService` (sync state in `rswiki.cfg`),
- the **RsWiki** page in the main window, its unread-count notifier, and the
  group / page / edit / moderator dialogs.

## Independent of the built-in wiki

RsWiki does not share anything with WikiPoos, so both can be enabled at the
same time:

| | built-in WikiPoos | RsWiki plugin |
|---|---|---|
| GXS service id | `RsServiceType::WIKI` (`0x0213`) | `RS_SERVICE_TYPE_RSWIKI_PLUGIN` (`0xc6e5`) |
| service name | `gxswiki` | `rswiki` |
| database | `gxs/wiki_db` | `gxs/rswiki_db` |
| config file | `wiki.cfg` | `rswiki.cfg` |
| rsEvents type | `"GXS_WIKI"` | `"RSWIKI_PLUGIN"` |
| GUI settings group | `WikiDialog`, notify `Wiki` | `RsWikiDialog`, notify `RsWiki` |
| C++ symbols | global `rsWiki`, `p3Wiki`, `WikiDialog`, … | `namespace rswiki`; forms `Ui::RsWiki*` |

The other side of that: RsWiki wikis only sync with friends who run the RsWiki
plugin, not with nodes using the built-in WikiPoos service, and existing
`wiki_db` content is not picked up. Since the protocol is separate, the old
snapshot quirk (page text sent twice, hash tags never sent) is fixed here:
`mHashTags` is serialised properly.

## Layout

| Path | Content |
|---|---|
| `RsWikiPlugin.*` | `RsPlugin` entry point: creates the store, service and net service, provides the page |
| `interface/rsWiki.h` | public `rswiki::RsWiki` API, service id, event type name and the `rswiki::rsWiki` global |
| `services/p3RsWiki.*` | `rswiki::p3Wiki` GXS service |
| `services/rsWikiItems.*` | serialised GXS items |
| `gui/` | `WikiDialog` (main page), `WikiAddDialog`, `WikiEditDialog`, `WikiGroupDialog`, `WikiModeratorChooserDialog`, `WikiUserNotify` |

Every dialog layout is a Designer `.ui` file (`gui/RsWiki*.ui`), including the
moderators box injected into the group dialog (`RsWikiModeratorsWidget.ui`) and
the moderator chooser (`RsWikiModeratorChooserDialog.ui`).

## Building

qmake:

```
cd plugins/RsWiki
qmake RsWiki.pro
make
```

The plugin is written to `lib/`. It needs `ui_GxsGroupDialog.h` from a built
retroshare-gui (`retroshare-gui/src/temp/ui`).

CMake, standalone (Windows needs the import libraries of a RetroShare build):

```
cmake -S plugins/RsWiki -B plugins/RsWiki/build -DRS_BUILD_DIR=<RetroShare build dir>
cmake --build plugins/RsWiki/build
```

or from the super-project with `-DRS_PLUGIN_RSWIKI=ON` (opt-in, off by default).

Copy the resulting library into the RetroShare plugins directory and enable it
under *Options → Plugins*.

## Notes

- **The wiki database is not encrypted.** RetroShare opens its own GXS
  databases (forums, channels, …) with the login password as SQLCipher key,
  but plugins are not given that key (`RsPlugInInterfaces` has no password).
  So – like RetroGit – RsWiki opens `gxs/rswiki_db` with an empty key, which
  SQLCipher treats as a plain SQLite file; RetroShare logs "The database is
  not encrypted" for it. Anyone with access to the profile folder can read
  the wiki pages stored there. Encrypting it would need libretroshare to pass
  the GXS database key to plugins.
