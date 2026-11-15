// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UIZOOMCONTROLLER_H
#define UIZOOMCONTROLLER_H

#include <QList>
#include <QObject>

QT_BEGIN_NAMESPACE
class QEvent;
class QSettings;
QT_END_NAMESPACE

/**
 * Selects, steps, and persists the global UI zoom exposed to QML as \c cppUiZoom.
 *
 * The zoom is a percentage taken from a fixed ladder of steps (50 % to 200 %,
 * 100 % being the default), similar to the zoom levels of web browsers. The main
 * window applies zoomFactor() to one scaling layer that hosts the whole UI, so
 * every screen, menu, and dialog follows the same value.
 *
 * Besides the keyboard shortcuts handled in QML, attachToWindow() lets Ctrl +
 * mouse wheel step the zoom, as in web browsers and code editors.
 */
class UiZoomController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(UiZoomController)

    /** Active zoom in percent, always one of zoomSteps(). */
    Q_PROPERTY(int zoomPercent READ zoomPercent WRITE setZoomPercent NOTIFY zoomChanged)

    /** Active zoom as a scale factor, for example \c 1.25 for 125 %. */
    Q_PROPERTY(qreal zoomFactor READ zoomFactor NOTIFY zoomChanged)

    /** Whether a larger zoom step is available. */
    Q_PROPERTY(bool canZoomIn READ canZoomIn NOTIFY zoomChanged)

    /** Whether a smaller zoom step is available. */
    Q_PROPERTY(bool canZoomOut READ canZoomOut NOTIFY zoomChanged)

    /** Whether the zoom is at the default 100 %. */
    Q_PROPERTY(bool isDefaultZoom READ isDefaultZoom NOTIFY zoomChanged)

public:
    /** Default zoom in percent. */
    static constexpr int DefaultZoomPercent = 100;

    /** Creates the controller and restores the saved zoom from \a settings (may be null). */
    explicit UiZoomController(QSettings *settings, QObject *parent = nullptr);

    /** Returns the active zoom in percent. */
    int zoomPercent() const;

    /** Returns the active zoom as a scale factor. */
    qreal zoomFactor() const;

    /** Returns whether a larger zoom step is available. */
    bool canZoomIn() const;

    /** Returns whether a smaller zoom step is available. */
    bool canZoomOut() const;

    /** Returns whether the zoom is at the default 100 %. */
    bool isDefaultZoom() const;

    /** Returns the selectable zoom steps in percent, in ascending order. */
    static QList<int> zoomSteps();

    /** Returns the zoom step nearest to \a percent. */
    static int nearestStep(int percent);

    /** Applies the step nearest to \a percent and persists it. */
    void setZoomPercent(int percent);

    /** Switches to the next larger zoom step; does nothing at the largest step. */
    Q_INVOKABLE void zoomIn();

    /** Switches to the next smaller zoom step; does nothing at the smallest step. */
    Q_INVOKABLE void zoomOut();

    /** Restores the default 100 % zoom. */
    Q_INVOKABLE void resetZoom();

    /** Lets Ctrl + mouse wheel over \a window step the zoom; \a window may be null. */
    void attachToWindow(QObject *window);

    /** Steps the zoom for Ctrl + wheel events on the attached window and consumes them. */
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    /** Emitted when the active zoom changes. */
    void zoomChanged();

private:
    /** Non-owning INI settings store used to persist the zoom. */
    QSettings *m_settings {nullptr};

    /** Active zoom in percent. */
    int m_zoomPercent {DefaultZoomPercent};

    /** Ctrl + wheel rotation not yet turned into a zoom step, in eighths of a degree. */
    int m_pendingWheelDelta {0};
};

#endif // UIZOOMCONTROLLER_H
