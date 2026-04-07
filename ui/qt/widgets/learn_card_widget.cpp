/* learn_card_widget.cpp
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <epan/prefs.h>

#include <ui/qt/widgets/learn_card_widget.h>
#include <ui/qt/utils/color_utils.h>
#include <ui/qt/utils/software_update.h>
#include <ui/qt/main_application.h>

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QHBoxLayout>
#include <QFrame>

#include "ui/urls.h"

LearnCardWidget::LearnCardWidget(QWidget *parent) :
    QFrame(parent)
    , main_layout_(nullptr)
    , link_container_(nullptr)
    , compact_link_container_(nullptr)
    , links_collapsed_(false)
{
    links_ = {
        {
            "https://www.wireshark.org/docs/wsug_html_chunked/",
            tr("User Documentation"),
            tr("Docs"),
            tr("Read the Wireshark user documentation online.")
        },
        {
            "https://gitlab.com/wireshark/wireshark/-/wikis/",
            tr("Wiki"),
            tr("Wiki"),
            tr("Browse the Wireshark Wiki for how-tos and other information.")
        },
        {
            "https://ask.wireshark.org/",
            tr("Questions and Answers"),
            tr("Q&A"),
            tr("Get answers to your Wireshark questions from the community.")
        },
        {
            "https://www.wireshark.org/lists/",
            tr("Mailing Lists"),
            tr("Lists"),
            tr("Join the Wireshark mailing lists to discuss Wireshark with other users and developers.")
        }
    };

    connect(SoftwareUpdate::instance(), &SoftwareUpdate::updateAvailable, this, &LearnCardWidget::setVersionInfo);
    connect(SoftwareUpdate::instance(), &SoftwareUpdate::updateEngaged, this, &LearnCardWidget::resetVersionInfo);
    connect(mainApp, &MainApplication::appInitialized, this, &LearnCardWidget::setupLayout);

    setupLayout();
}

void LearnCardWidget::setupLayout()
{
    if (main_layout_) {
        delete main_layout_;
        qDeleteAll(findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));

        main_layout_ = nullptr;
    }

    main_layout_ = new QVBoxLayout(this);
    setLayout(main_layout_);
    main_layout_->setContentsMargins(0, 0, 0, 0);
    main_layout_->setSpacing(0);

    setupHeader();
    setupLinks();
    setupUpdateInfo();
    setupActionButtons();
}

void LearnCardWidget::setupHeader()
{
    ClickableLabel * header_label_ = new ClickableLabel(this);
    header_label_->setObjectName("learnHeader");
    header_label_->setText(tr("<h2>Learn</h2>"));
    header_label_->setAccessibleName(tr("Learn"));
    header_label_->setAccessibleDescription(tr("Opens the Wireshark documentation website"));
    header_label_->setContentsMargins(16, 12, 16, 12);
    header_label_->setCursor(Qt::PointingHandCursor);
    connect(header_label_, &ClickableLabel::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(WS_DOCS_URL));
    });
    main_layout_->addWidget(header_label_);

    QFrame * header_separator_ = new QFrame(this);
    header_separator_->setObjectName("learnHeaderSeparator");
    header_separator_->setFrameShape(QFrame::HLine);
    header_separator_->setFrameShadow(QFrame::Plain);
    header_separator_->setFixedHeight(1);
    main_layout_->addWidget(header_separator_);
}

void LearnCardWidget::setupLinks()
{
    // Full vertical link list
    link_container_ = new QWidget(this);
    link_container_->setObjectName("learnLinkContainer");
    QVBoxLayout *link_layout = new QVBoxLayout(link_container_);
    link_layout->setContentsMargins(16, 4, 16, 4);
    link_layout->setSpacing(0);

    foreach (const learn_link_t &link, links_) {
        QLabel *link_label = new QLabel(this);
        link_label->setObjectName("learnLink");
        link_label->setTextFormat(Qt::RichText);
        link_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        link_label->setOpenExternalLinks(true);
        link_label->setText(QString("<a href=\"%1\" title=\"%2\">%3</a>")
                           .arg(link.url, link.tooltip, link.label));
        link_label->setAccessibleName(link.label);
        link_label->setAccessibleDescription(link.tooltip);
        link_label->setContentsMargins(10, 4, 10, 4);
        link_layout->addWidget(link_label);
    }

    main_layout_->addWidget(link_container_);

    // Compact horizontal link row (shown when collapsed)
    compact_link_container_ = new QWidget(this);
    compact_link_container_->setObjectName("learnCompactLinkContainer");
    QHBoxLayout *compact_layout = new QHBoxLayout(compact_link_container_);
    compact_layout->setContentsMargins(16, 4, 16, 4);
    compact_layout->setSpacing(4);

    foreach (const learn_link_t &link, links_) {
        QLabel *link_label = new QLabel(this);
        link_label->setObjectName("learnLink");
        link_label->setTextFormat(Qt::RichText);
        link_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        link_label->setOpenExternalLinks(true);
        link_label->setText(QString("<a href=\"%1\" title=\"%2\">%3</a>")
                           .arg(link.url, link.tooltip, link.short_label));
        // Use the full label as the accessible name, not short_label, so screen
        // readers announce e.g. "User Documentation" rather than "Docs" when
        // the card is in collapsed/compact mode.
        link_label->setAccessibleName(link.label);
        link_label->setAccessibleDescription(link.tooltip);
        link_label->setContentsMargins(4, 4, 4, 4);
        compact_layout->addWidget(link_label);
    }
    compact_layout->addStretch();

    compact_link_container_->setVisible(false);
    main_layout_->addWidget(compact_link_container_);

    main_layout_->addStretch(1);
}

void LearnCardWidget::setVersionInfo(QString newVersion)
{
    new_version_ = newVersion;
    setupUpdateInfo();
}

void LearnCardWidget::resetVersionInfo()
{
    new_version_.clear();
    setupUpdateInfo();
}

void LearnCardWidget::setupUpdateInfo()
{
    if (!SoftwareUpdate::plattformSupported())
        return;

    QLabel * update_info_label = findChild<QLabel*>("learnUpdateInfoLabel");
    QPushButton * update_button = findChild<QPushButton*>("learnUpdateButton");
    /* reusing the label if it already exists, avoids having to define
     * a label in the header */
    if (!update_info_label) {
        QWidget *update_info_container = new QWidget(this);
        update_info_container->setObjectName("learnUpdateInfoContainer");
        QHBoxLayout *update_layout = new QHBoxLayout(update_info_container);
        update_layout->setContentsMargins(16, 4, 16, 4);
        update_layout->setSpacing(0);

        update_info_label = new QLabel(update_info_container);
        update_info_label->setObjectName("learnUpdateInfoLabel");
        update_info_label->setAccessibleName(tr("Software Update Information"));
        update_info_label->setAccessibleDescription(tr("Information if a new update is available and if automatic updates are enabled"));
        update_info_label->setVisible(false);
        update_layout->addWidget(update_info_label);

        update_button = new QPushButton(tr("Check for Updates"), update_info_container);
        update_button->setObjectName("learnUpdateButton");
        update_button->setToolTip(tr("Install a new software update"));
        update_button->setAccessibleDescription(tr("This button is only shown when an update is available."));
        update_button->setCursor(Qt::PointingHandCursor);
        update_button->setVisible(false);
        connect(update_button, &QPushButton::clicked, this, []() {
            SoftwareUpdate::instance()->performUIUpdate();
        });
        update_layout->addWidget(update_button);

        main_layout_->addWidget(update_info_container);
    } else if (!prefs.gui_update_enabled && SoftwareUpdate::plattformSupported()) {
        /** If automatic updates are disabled and the platform supports updates,
         *  show a warning message in the label to inform the user that they won't
         *  receive updates. This is set in this else path, as we only want to show
         *  the info if the pref is set, which we only have knowledge about AFTER
         *  the main app initialization.
         */
        update_info_label->setVisible(true);
        update_info_label->setText(tr(" You have disabled automatic updates."));
    }

    if (prefs.gui_update_enabled) {
        update_button->setVisible(false);
        update_info_label->setVisible(false);

        /* update the label text based on the current version and update information */
        if (!new_version_.isEmpty()) {
            update_button->setVisible(true);
            update_button->setText(tr(" A new version (%1) is available.").arg(new_version_));
        } else {
            update_info_label->setVisible(true);
            update_info_label->setText(tr(" You receive automatic updates."));
        }
    }
}

void LearnCardWidget::setupActionButtons()
{
    QWidget *button_container = new QWidget(this);
    button_container->setObjectName("learnButtonContainer");
    QHBoxLayout *button_layout = new QHBoxLayout(button_container);
    button_layout->setContentsMargins(16, 12, 16, 12);
    button_layout->setSpacing(8);

    QPushButton * discord_button_ = new QPushButton(tr("Discord"), this);
    discord_button_->setObjectName("discord");
    discord_button_->setToolTip(tr("Join the Wireshark Discord server to chat with other users and developers."));
    discord_button_->setAccessibleDescription(tr("Join the Wireshark Discord server to chat with other users and developers."));
    discord_button_->setCursor(Qt::PointingHandCursor);
    connect(discord_button_, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://discord.gg/fT2jvkawGj"));
    });
    button_layout->addWidget(discord_button_);

    QPushButton * donate_button_ = new QPushButton(tr("Donate"), this);
    donate_button_->setObjectName("donate");
    donate_button_->setToolTip(tr("Support the Wireshark project by making a donation to the Wireshark Foundation."));
    donate_button_->setAccessibleDescription(tr("Support the Wireshark project by making a donation to the Wireshark Foundation."));
    donate_button_->setCursor(Qt::PointingHandCursor);
    connect(donate_button_, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://wiresharkfoundation.org/donate/"));
    });
    button_layout->addWidget(donate_button_);

    main_layout_->addWidget(button_container);
}

void LearnCardWidget::setLinksCollapsed(bool collapsed)
{
    if (links_collapsed_ == collapsed)
        return;
    links_collapsed_ = collapsed;
    if (link_container_)
        link_container_->setVisible(!collapsed);
    if (compact_link_container_)
        compact_link_container_->setVisible(collapsed);
}

bool LearnCardWidget::isLinksCollapsed() const
{
    return links_collapsed_;
}

void LearnCardWidget::updateStyleSheets(const QColor &header_text_color, const QColor &header_hover_color)
{
    QPalette palette = QApplication::palette();
    QColor base_color = palette.color(QPalette::Base);
    QColor border_color = ColorUtils::themeIsDark()
        ? base_color.lighter(200)
        : base_color.darker(120);
    QColor card_bg = ColorUtils::themeIsDark()
        ? base_color.lighter(120)
        : base_color.darker(105);
    QColor hover_bg = ColorUtils::hoverBackground();
    QString link_style = ColorUtils::themeLinkStyle();

    QString styleSheet;

    styleSheet.append(QStringLiteral(
            "LearnCardWidget {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 6px;"
            "}"
            )
            .arg(card_bg.name(), border_color.name()));

    styleSheet.append(QStringLiteral(
            "QLabel#learnHeader {"
            "  color: %1;"
            "  padding: 10px 12px;"
            "}"
            "QLabel#learnHeader:hover {"
            "  color: %2;"
            "}"
            )
            .arg(header_text_color.name(), header_hover_color.name()));

    styleSheet.append(QStringLiteral(
            "QFrame#learnHeaderSeparator {"
            "  color: %1;"
            "}"
            )
            .arg(border_color.name()));

    styleSheet.append(QStringLiteral(
            "QLabel#learnLink {"
            "  border-radius: 4px;"
            "}"
            "QLabel#learnLink:hover {"
            "  background-color: %1;"
            "}"
            "%2"
            )
            .arg(hover_bg.name(QColor::HexArgb), link_style));

    // Update button: brand purple #5865F2
    styleSheet.append(QStringLiteral(
            "QPushButton#learnUpdateButton {"
            "  background-color: #5865F2;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 6px;"
            "  padding: 10px 12px;"
            "  font-weight: 500;"
            "}"
            "QPushButton#learnUpdateButton:hover {"
            "  background-color: #4752C4;"
            "}"
            ));


    // Discord button: brand purple #5865F2
    styleSheet.append(QStringLiteral(
            "QPushButton#discord {"
            "  background-color: #5865F2;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 6px;"
            "  padding: 10px 12px;"
            "  font-weight: 500;"
            "}"
            "QPushButton#discord:hover {"
            "  background-color: #4752C4;"
            "}"
            ));

    // Donate button: warm red
    styleSheet.append(QStringLiteral(
            "QPushButton#donate {"
            "  background-color: #C0392B;"
            "  color: white;"
            "  border: none;"
            "  border-radius: 6px;"
            "  padding: 10px 12px;"
            "  font-weight: 500;"
            "}"
            "QPushButton#donate:hover {"
            "  background-color: #A93226;"
            "}"
            ));

    // Button container top border (separator)
    styleSheet.append(QStringLiteral(
                "QWidget#learnButtonContainer {"
                "  border-top: 1px solid %1;"
                "}"
                )
                .arg(border_color.name()));

    setStyleSheet(styleSheet);
}

