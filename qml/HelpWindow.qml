// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype HelpWindow
    \inqmlmodule ReqDeck
    \brief A window with the user manual: its pages on the left, the page on the right.

    The pages are the Markdown files of \c cppManual, the same files the GitHub wiki
    is made of, shown by Qt's own Markdown support. Links between pages open the
    page and scroll to the section; web addresses open in the system browser,
    never in this window. Back and Forward walk the pages seen, and Ctrl+F finds
    text in the page.
*/
ApplicationWindow {
    id: root

    /*! Whether the window follows the dark theme. */
    property bool darkTheme: false

    /*! Id of the page shown. */
    property string pageId: ""

    /*! Whether pages get the ReqDeck look; false shows Qt's plain Markdown. */
    property bool styled: true

    /*! Pages seen before the current one, as {page, anchor} entries. */
    property var backStack: []

    /*! Pages left by Back, as {page, anchor} entries. */
    property var forwardStack: []

    /*! Number of places the find text occurs in the page. */
    property int matchCount: 0

    /*! Index of the selected match, from 0. */
    property int matchIndex: -1

    /*!
        Shows the page \a page and scrolls to its section \a anchor, if any.
        The page left goes onto the Back list unless \a fromHistory is true.
    */
    function openPage(page, anchor, fromHistory) {
        if (cppManual.markdown(page).length === 0)
            page = "Home"
        if (!fromHistory && root.pageId.length > 0 && page !== root.pageId) {
            root.backStack = root.backStack.concat([{ page: root.pageId, anchor: "" }])
            root.forwardStack = []
        }
        root.pageId = page
        pageText.text = cppManual.markdown(page)
        if (root.styled)
            cppManual.decorate(pageText.textDocument, root.darkTheme, pageText.width)
        root.scrollTo(anchor ? anchor : "")
        root.findText()
    }

    /*! Scrolls to the heading \a anchor of the shown page, or to the top. */
    function scrollTo(anchor) {
        const position = anchor.length > 0 ? cppManual.anchorIn(pageText.textDocument, anchor) : -1
        if (position < 0) {
            pageFlick.contentY = 0
            return
        }
        pageText.cursorPosition = position
        const top = pageText.positionToRectangle(position).y
        pageFlick.contentY = Math.max(0, Math.min(top, pageFlick.contentHeight - pageFlick.height))
    }

    /*!
        Follows the link \a link of the shown page: a page of the manual opens here,
        a web address in the system browser, anything else is ignored.
    */
    function followLink(link) {
        const target = cppManual.resolve(link, root.pageId)
        if (target.kind === "page")
            root.openPage(target.page, target.anchor, false)
        else if (target.kind === "external")
            Qt.openUrlExternally(target.url)
    }

    /*! Shows the page seen before the current one. */
    function goBack() {
        if (root.backStack.length === 0)
            return
        const entry = root.backStack[root.backStack.length - 1]
        root.backStack = root.backStack.slice(0, -1)
        root.forwardStack = root.forwardStack.concat([{ page: root.pageId, anchor: "" }])
        root.openPage(entry.page, entry.anchor, true)
    }

    /*! Shows the page left by Back. */
    function goForward() {
        if (root.forwardStack.length === 0)
            return
        const entry = root.forwardStack[root.forwardStack.length - 1]
        root.forwardStack = root.forwardStack.slice(0, -1)
        root.backStack = root.backStack.concat([{ page: root.pageId, anchor: "" }])
        root.openPage(entry.page, entry.anchor, true)
    }

    /*! Counts the find text in the page and selects the match \a index. */
    function findText(index) {
        const needle = findField.text.toLowerCase()
        const haystack = pageText.getText(0, pageText.length).toLowerCase()
        const starts = []
        if (needle.length > 0) {
            for (let at = haystack.indexOf(needle); at >= 0 && starts.length < 10000; at = haystack.indexOf(needle, at + needle.length))
                starts.push(at)
        }
        root.matchCount = starts.length
        if (starts.length === 0) {
            root.matchIndex = -1
            pageText.deselect()
            return
        }
        root.matchIndex = ((index === undefined ? 0 : index) % starts.length + starts.length) % starts.length
        const start = starts[root.matchIndex]
        pageText.select(start, start + needle.length)
        const top = pageText.positionToRectangle(start).y
        if (top < pageFlick.contentY || top > pageFlick.contentY + pageFlick.height - 40)
            pageFlick.contentY = Math.max(0, Math.min(top - 40, pageFlick.contentHeight - pageFlick.height))
    }

    width: 1000
    height: 700
    minimumWidth: 560
    minimumHeight: 360
    title: qsTr("ReqDeck User Manual")
    Material.theme: root.darkTheme ? Material.Dark : Material.Light
    // The page colours follow the theme, so the page is laid out again.
    onDarkThemeChanged: if (root.pageId.length > 0) root.openPage(root.pageId, "", true)
    Material.accent: Base.BsTheme.accent

    // The tool bar has the ground of the menu band of the main window.
    header: ToolBar {
        background: Rectangle {
            color: Base.BsTheme.menuBandColor
        }

        RowLayout {
            anchors.fill: parent
            spacing: 2

            ToolButton {
                objectName: "helpBack"
                display: AbstractButton.IconOnly
                icon.source: "qrc:/images/svg/arrow_back.svg"
                icon.width: 24
                icon.height: 24
                icon.color: enabled ? Base.BsTheme.accent : Qt.rgba(Material.foreground.r, Material.foreground.g, Material.foreground.b, 0.35)
                enabled: root.backStack.length > 0
                Accessible.name: qsTr("Back")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Back")
                onClicked: root.goBack()
            }

            ToolButton {
                objectName: "helpForward"
                display: AbstractButton.IconOnly
                icon.source: "qrc:/images/svg/arrow_forward.svg"
                icon.width: 24
                icon.height: 24
                icon.color: enabled ? Base.BsTheme.accent : Qt.rgba(Material.foreground.r, Material.foreground.g, Material.foreground.b, 0.35)
                enabled: root.forwardStack.length > 0
                Accessible.name: qsTr("Forward")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Forward")
                onClicked: root.goForward()
            }

            Item { Layout.fillWidth: true }

            TextField {
                id: findField

                objectName: "helpFind"

                Layout.preferredWidth: 220
                placeholderText: qsTr("Find in page")
                Accessible.name: qsTr("Find in page")
                onTextEdited: root.findText(0)
                onAccepted: root.findText(root.matchIndex + 1)
            }

            Label {
                Layout.rightMargin: 8
                text: findField.text.length === 0 ? ""
                      : root.matchCount === 0 ? qsTr("No matches")
                      : qsTr("%1 of %2").arg(root.matchIndex + 1).arg(root.matchCount)
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: findField.forceActiveFocus()
    }

    Shortcut {
        sequence: "F3"
        enabled: root.matchCount > 0
        onActivated: root.findText(root.matchIndex + 1)
    }

    Shortcut {
        sequence: "Shift+F3"
        enabled: root.matchCount > 0
        onActivated: root.findText(root.matchIndex - 1)
    }

    Shortcut {
        sequences: [StandardKey.Back]
        onActivated: root.goBack()
    }

    Shortcut {
        sequences: [StandardKey.Forward]
        onActivated: root.goForward()
    }

    Timer {
        id: relayout

        interval: 150
        onTriggered: {
            if (root.pageId.length === 0 || !root.styled)
                return
            const ratio = pageFlick.contentHeight > 0 ? pageFlick.contentY / pageFlick.contentHeight : 0
            pageText.text = cppManual.markdown(root.pageId)
            cppManual.decorate(pageText.textDocument, root.darkTheme, pageText.width)
            pageFlick.contentY = ratio * pageFlick.contentHeight
        }
    }

    Connections {
        target: cppManual

        // The same page in the new language, from its top.
        function onLanguageChanged() {
            root.openPage(root.pageId, "", true)
        }
    }

    SplitView {
        anchors.fill: parent

        ListView {
            id: pageList

            objectName: "helpPages"

            SplitView.preferredWidth: 220
            SplitView.minimumWidth: 140
            clip: true
            model: cppManual.pages
            Accessible.name: qsTr("Pages of the manual")

            delegate: ItemDelegate {
                required property var modelData

                width: ListView.view.width
                text: modelData.title
                highlighted: modelData.id === root.pageId
                onClicked: root.openPage(modelData.id, "", false)
            }
        }

        Flickable {
            id: pageFlick

            SplitView.fillWidth: true
            contentWidth: width
            contentHeight: pageText.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {}

            TextArea.flickable: TextArea {
                id: pageText

                // Images are fitted to the width the page has, so the page is
                // laid out again when the window gets its size or is resized.
                onWidthChanged: relayout.restart()

                objectName: "helpText"

                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.MarkdownText
                baseUrl: cppManual.baseUrl
                leftPadding: 24
                rightPadding: 24
                topPadding: 16
                bottomPadding: 24
                Accessible.name: qsTr("Page of the manual")
                onLinkActivated: (link) => root.followLink(link)
            }
        }
    }
}
