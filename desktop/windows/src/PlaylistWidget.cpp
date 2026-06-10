#include "PlaylistWidget.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileInfo>

PlaylistWidget::PlaylistWidget(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(200);
    setMaximumWidth(400);
    setStyleSheet("background:#0D0D12;");
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0,0,0,0);
    lay->setSpacing(0);

    auto* hdr = new QLabel(" Playlist");
    hdr->setStyleSheet("background:#14141A; color:#999; padding:6px; font-size:12px;");
    hdr->setFixedHeight(28);

    m_list = new QListWidget;
    m_list->setStyleSheet(R"(
        QListWidget { background:#0D0D12; color:#CCC; border:none; }
        QListWidget::item:selected { background:#007ACC; color:#FFF; }
        QListWidget::item:hover { background:#1E1E2A; }
    )");

    lay->addWidget(hdr);
    lay->addWidget(m_list, 1);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        emit itemActivated(item->data(Qt::UserRole).toString());
    });
}

void PlaylistWidget::addItem(const QString& path) {
    auto* item = new QListWidgetItem(QFileInfo(path).fileName());
    item->setData(Qt::UserRole, path);
    item->setToolTip(path);
    m_list->addItem(item);
    m_list->setCurrentItem(item);
}

void PlaylistWidget::clear() { m_list->clear(); }
