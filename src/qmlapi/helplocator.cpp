// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/helplocator.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>

namespace {
/*!
 * \internal
 * \brief Returns the topmost visible item with a help topic at \a scenePoint below
 * \a item, or null.
 *
 * Children are tried from the top of the stacking order down, and a child may
 * lie outside its parent, as popups in the overlay do. An item without a topic
 * does not end the search: the overlay for popups covers the whole window and
 * has none, and neither has the dimmer of a modal dialog. Disabled items count:
 * a greyed-out button still deserves an explanation.
 */
QQuickItem *itemAt(QQuickItem *item, const QPointF &scenePoint)
{
    if (!item->isVisible() || qFuzzyIsNull(item->opacity()))
        return nullptr;

    QList<QQuickItem *> children = item->childItems();
    std::stable_sort(children.begin(), children.end(),
                     [](const QQuickItem *left, const QQuickItem *right) {
                         return left->z() < right->z();
                     });
    for (auto it = children.crbegin(); it != children.crend(); ++it) {
        if (QQuickItem *hit = itemAt(*it, scenePoint))
            return hit;
    }
    if (!item->contains(item->mapFromScene(scenePoint)))
        return nullptr;
    return HelpLocator::topicOf(item).isEmpty() ? nullptr : item;
}
} // namespace

/*!
 * \brief Creates a locator and starts watching the input of the application.
 * \param parent The QObject parent.
 */
HelpLocator::HelpLocator(QObject *parent)
    : QObject(parent)
{
    if (QCoreApplication *application = QCoreApplication::instance())
        application->installEventFilter(this);
}

/*!
 * \brief Returns the topic of the element at \a point.
 * \param window The window.
 * \param point A position in the scene of \a window.
 * \return The topic, or an empty string when no element there has one.
 */
QString HelpLocator::topicAt(QQuickWindow *window, const QPointF &point) const
{
    if (!window || !window->contentItem())
        return {};
    // The overlay with popups and dialogs is a sibling of the content item.
    QQuickItem *root = window->contentItem()->parentItem() ? window->contentItem()->parentItem()
                                                           : window->contentItem();
    return topicOf(itemAt(root, point));
}

/*!
 * \brief Returns the topic of the focused element of \a window.
 * \param window The window.
 */
QString HelpLocator::topicForFocus(QQuickWindow *window) const
{
    return window ? topicOf(window->activeFocusItem()) : QString();
}

/*!
 * \brief Returns the topic F1 refers to in \a window.
 *
 * The last kind of input decides which comes first: after the mouse moved, the
 * element under the pointer; after a key press, the focused element. Otherwise
 * a pointer resting anywhere would override the field the user is typing in.
 * \param window The window.
 * \return The topic, or "start" when neither has one.
 */
QString HelpLocator::currentTopic(QQuickWindow *window) const
{
    const QString pointer = m_pointerWindow == window ? topicAt(window, m_pointerPosition) : QString();
    const QString focus = topicForFocus(window);
    const QString first = m_lastInput == Input::Keyboard ? focus : pointer;
    const QString second = m_lastInput == Input::Keyboard ? pointer : focus;
    if (!first.isEmpty())
        return first;
    if (!second.isEmpty())
        return second;
    return QStringLiteral("start");
}

/*!
 * \brief Records whether the mouse or the keyboard was used last.
 *
 * F1 itself does not count, so that it can explain what the mouse points at.
 * \param watched Unused.
 * \param event The event.
 * \return Always false: the event goes on.
 */
bool HelpLocator::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
        if (auto *window = qobject_cast<QQuickWindow *>(watched)) {
            m_lastInput = Input::Mouse;
            m_pointerWindow = window;
            m_pointerPosition = static_cast<QMouseEvent *>(event)->scenePosition();
        }
        break;
    case QEvent::KeyPress:
        if (static_cast<QKeyEvent *>(event)->key() != Qt::Key_F1)
            m_lastInput = Input::Keyboard;
        break;
    default:
        break;
    }
    return false;
}

/*!
 * \brief Returns the topic of \a object or of its nearest ancestor that has one.
 *
 * A popup is not an item, so the walk from its content goes from the popup's own
 * item to the popup, and from there to the item it was declared in. That lets a
 * dialog or a menu name one topic for all it holds.
 * \param object Usually an item.
 * \return The topic, or an empty string.
 */
QString HelpLocator::topicOf(const QObject *object)
{
    while (object) {
        const QString topic = object->property("helpTopic").toString();
        if (!topic.isEmpty())
            return topic;
        const auto *item = qobject_cast<const QQuickItem *>(object);
        if (item && item->parentItem() && !item->inherits("QQuickPopupItem"))
            object = item->parentItem();
        else
            object = object->parent();
    }
    return {};
}
