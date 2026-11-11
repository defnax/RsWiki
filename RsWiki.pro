!include("../Common/retroshare_plugin.pri"): error("Could not include file ../Common/retroshare_plugin.pri")

# Generated files must never be shared between Qt major versions or build modes.
CONFIG(debug, debug|release) {
	RSWIKI_BUILD_MODE = debug
} else {
	RSWIKI_BUILD_MODE = release
}
RSWIKI_GENERATED_DIR = $$OUT_PWD/temp/qt$${QT_MAJOR_VERSION}/$${RSWIKI_BUILD_MODE}
OBJECTS_DIR = $$RSWIKI_GENERATED_DIR/obj
MOC_DIR = $$RSWIKI_GENERATED_DIR/moc
RCC_DIR = $$RSWIKI_GENERATED_DIR/qrc
UI_DIR = $$RSWIKI_GENERATED_DIR/ui

greaterThan(QT_MAJOR_VERSION, 4) {
	# Qt 5 and Qt 6
	QT += widgets
}

CONFIG += qt uic qrc resources
CONFIG += c++17
DESTDIR = lib
TARGET = RsWiki

# RsWiki runs its own GXS service (own service id, database, config and event
# type, code in namespace rswiki), so it can coexist with the in-core
# WikiPoos service (CONFIG += wikipoos / RS_USE_WIKI).

INCLUDEPATH += $$PWD
DEPENDPATH  += $$PWD

# ui_GxsGroupDialog.h (WikiGroupDialog derives from GxsGroupDialog)
DEPENDPATH  += ../../retroshare-gui/src/temp/ui ../../libretroshare/src
INCLUDEPATH += ../../retroshare-gui/src/temp/ui ../../libretroshare/src
INCLUDEPATH += ../../../Build/retroshare-gui/src/temp/ui
INCLUDEPATH += ../../retroshare-gui/src/retroshare-gui

INCLUDEPATH += ../../rapidjson-1.1.0

#################################### Windows #####################################

win32 {
	# Use the toolchain's CRT, matching RetroShare.
}

QMAKE_CXXFLAGS *= -Wall

# Keep the library loaded when RetroShare dlclose()s it at shutdown (the
# Windows build pins itself in RsWikiPlugin.cpp).
linux-*|freebsd-*|openbsd-* {
	QMAKE_LFLAGS += -Wl,-z,nodelete
}

# RapidJSON 1.1.0 contains an assignment operator that modifies const members.
# GCC 15 checks this eagerly in template bodies, even when it is never used.
greaterThan(QMAKE_GCC_MAJOR_VERSION, 14) {
	QMAKE_CXXFLAGS += -Wno-template-body
}

# Markdown rendering: RsWiki carries its own copy of RetroShare's Markdown
# editor and renderer (gui/markdown), so it builds against RetroShare versions
# without them. By default pages are rendered with Qt's Markdown parser
# (Qt >= 5.14). For the same rendering as RetroShare's cmark-based posts:
#     qmake CONFIG+=rswiki_cmark
# (MSYS2: pacman -S mingw-w64-x86_64-cmark, Debian/Ubuntu: libcmark-dev)
rswiki_cmark {
	DEFINES *= RSWIKI_USE_CMARK
	linux-*|freebsd-*|openbsd-* {
		CONFIG *= link_pkgconfig
		PKGCONFIG *= libcmark
	} else {
		LIBS *= -lcmark
	}
}

################################### HEADERS & SOURCES #############################

SOURCES = RsWikiPlugin.cpp \
          services/p3RsWiki.cc \
          services/rsWikiItems.cc \
          gui/WikiDialog.cpp \
          gui/WikiSpaceCard.cpp \
          gui/WikiCommentItem.cpp \
          gui/WikiEditDialog.cpp \
          gui/WikiGroupDialog.cpp \
          gui/WikiModeratorChooserDialog.cpp \
          gui/WikiUserNotify.cpp \
          gui/markdown/MarkdownTextEdit.cpp \
          gui/markdown/MarkdownSyntaxHighlighter.cpp \
          gui/markdown/RsMarkdown.cpp

HEADERS = RsWikiPlugin.h \
          interface/rsWiki.h \
          services/p3RsWiki.h \
          services/rsWikiItems.h \
          gui/WikiDialog.h \
          gui/WikiStyle.h \
          gui/WikiSpaceCard.h \
          gui/WikiCommentItem.h \
          gui/WikiEditDialog.h \
          gui/WikiGroupDialog.h \
          gui/WikiModeratorChooserDialog.h \
          gui/WikiUserNotify.h \
          gui/markdown/MarkdownTextEdit.h \
          gui/markdown/MarkdownSyntaxHighlighter.h \
          gui/markdown/RsMarkdown.h

FORMS += gui/RsWikiDialog.ui \
         gui/RsWikiSpaceCard.ui \
         gui/RsWikiCommentItem.ui \
         gui/RsWikiEditDialog.ui \
         gui/RsWikiModeratorsWidget.ui \
         gui/RsWikiModeratorChooserDialog.ui

RESOURCES = gui/RsWiki_images.qrc
