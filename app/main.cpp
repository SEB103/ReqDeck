// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QGuiApplication>
#include <QString>

#include "productinfo.h"

// Minimal build-skeleton entry point: it only establishes the product identity
// from the generated productinfo.h and exits. The full application bootstrap
// (settings, AppPaths, QML engine) replaces it once the application layer exists.
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral(PRODUCT_IDENTIFIER));
    QCoreApplication::setOrganizationDomain(QStringLiteral(PRODUCT_ORG_DOMAIN));
    QCoreApplication::setApplicationName(QStringLiteral(PRODUCT_IDENTIFIER));
    QCoreApplication::setApplicationVersion(QStringLiteral(PRODUCT_VERSION));

    return 0;
}
