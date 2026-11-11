# RsWiki – RetroShare wiki plugin

A GXS wiki service, derived from the in-core **WikiPoos** service
(`CONFIG += wikipoos` / `RS_GXSWIKIPOS` / `RS_USE_WIKI`), packaged as a
loadable RetroShare plugin.

The plugin owns the whole service:

- its GXS data store (`gxs/rswiki_db`),
- the `p3Wiki` exchange service and its `RsGxsNetService` (sync state in `rswiki.cfg`),
- the **RsWiki** page in the main window, its unread-count notifier, and the
  group / page / edit / moderator dialogs.

## Features

### Wikis
- Create wikis (GXS groups) and share them with your friends; they spread
  through the RetroShare network like forums and channels
- Browse all wikis your friends share and subscribe to the ones you want
- Wiki details: name, description, distribution settings, subscriber count
- Category and tags per wiki; Browse all wikis filters by category, and
  `#tag` in the filter finds wikis by tag
- Moderators: the admin can add and remove moderators per wiki

### Pages
- Pages written in Markdown, rendered with links, RetroShare links and images
- Sub-pages, shown as a page tree in the sidebar
- Tags on pages (comma separated in the editor), shown under the title;
  clicking a tag lists every page with it
- Full edit history for every page
- Pages and new versions are signed with the wiki's publish key, so only the
  admin's node (or a friend it shared the key with) publishes them; there Edit
  publishes directly, signed by the page author, the wiki creator or a
  moderator. Everyone else's change is saved as an edit proposal
- Proposals are listed under their version in the edit history, where a
  moderator can publish one, merge several, or restore an older version
- New versions by others are checked against the page author and the
  moderator list; versions from anyone else are rejected
- All pages and edits are signed with a GXS identity

### Reading and finding
- Home page with your wikis, recently updated pages, your own pages
  and favorites
- Search across all pages (titles, content and tags); `#tag` finds pages
  with exactly that tag
- Page filter in each wiki's sidebar
- Unread pages are shown in bold; counts per wiki and in the RetroShare
  notification area; mark pages read or unread
- Favorites (stored locally)

### Comments
- Comment on any page with one of your identities
- Comments sync to everyone subscribed to the wiki

### Look
- Follows the active RetroShare theme (light, dark or custom stylesheet)
- Optional light skin (`gui/qss/RsWiki_light.qss`), user-overridable
- Per-theme skins via `qss/RsWiki/RsWiki_<Name>.qss`

### Technical
- Own GXS service (id `0xc6e5`), independent from the built-in WikiPoos;
  both can be enabled at the same time
- Note: the wiki database (`gxs/rswiki_db`) is not encrypted

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

### Markdown

RsWiki carries its own copy of RetroShare's Markdown editor and renderer
(`gui/markdown`: `MarkdownTextEdit`, `MarkdownSyntaxHighlighter`, `RsMarkdown`,
in namespace `rswiki`), so it builds against RetroShare versions that don't
have them yet. Pages are rendered with Qt's Markdown parser (Qt >= 5.14) by
default. For the same cmark-based rendering as RetroShare's posts, build with
`qmake CONFIG+=rswiki_cmark` or `-DRSWIKI_USE_CMARK=ON` (needs libcmark:
MSYS2 `mingw-w64-x86_64-cmark`, Debian/Ubuntu `libcmark-dev`).

## Notes

- **The wiki database is not encrypted.** RetroShare opens its own GXS
  databases (forums, channels, …) with the login password as SQLCipher key,
  but plugins are not given that key (`RsPlugInInterfaces` has no password).
  So – like RetroGit – RsWiki opens `gxs/rswiki_db` with an empty key, which
  SQLCipher treats as a plain SQLite file; RetroShare logs "The database is
  not encrypted" for it. Anyone with access to the profile folder can read
  the wiki pages stored there. Encrypting it would need libretroshare to pass
  the GXS database key to plugins.
