// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HELPLOCATOR_H
#define HELPLOCATOR_H

#include <QObject>
#include <QPointF>
#include <QPointer>

class QQuickItem;
class QQuickWindow;
class QWindow;

/**
 * Finds the help topic F1 refers to, exposed to QML as \c cppHelpLocator.
 *
 * Elements name their topic in a \c helpTopic property; an element without one
 * takes the topic of the nearest ancestor that has one. The locator watches the
 * input of the application: after a mouse movement F1 explains the element under
 * the pointer, after a key press the focused element.
 */
class HelpLocator : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HelpLocator)

public:
    /** Creates a locator and starts watching the input of the application. */
    explicit HelpLocator(QObject *parent = nullptr);

    /** Returns the topic of the element at \a point in the scene of \a window, or "". */
    Q_INVOKABLE QString topicAt(QQuickWindow *window, const QPointF &point) const;

    /** Returns the topic of the focused element of \a window, or "". */
    Q_INVOKABLE QString topicForFocus(QQuickWindow *window) const;

    /** Returns the topic F1 refers to in \a window, "start" when nothing has one. */
    Q_INVOKABLE QString currentTopic(QQuickWindow *window) const;

protected:
    /** Records whether the mouse or the keyboard was used last. */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /** Kind of the last input. */
    enum class Input {
        None,
        Mouse,
        Keyboard
    };

    /** Returns the topic of \a item or of its nearest ancestor that has one. */
    static QString topicOf(const QQuickItem *item);

    /** Kind of the last input. */
    Input m_lastInput {Input::None};

    /** Window of the last mouse movement. */
    QPointer<QWindow> m_pointerWindow;

    /** Position of the last mouse movement in the scene of that window. */
    QPointF m_pointerPosition;
};

#endif // HELPLOCATOR_H
