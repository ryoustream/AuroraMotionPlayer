#include "ChapterWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QFont>

static const char* kStyle = R"(
QWidget { background: #0D0D14; color: #E0E0F0; }
QLabel#title {
    font-size: 11px; font-weight: bold; color: #8888AA;
    padding: 4px 6px; letter-spacing: 1px;
    text-transform: uppercase;
}
QListWidget {
    background: transparent; border: none;
    outline: none; font-size: 12px;
}
QListWidget::item {
    padding: 6px 8px; border-bottom: 1px solid #1A1A2A;
    border-radius: 0px;
}
QListWidget::item:hover    { background: #1A1A2E; }
QListWidget::item:selected { background: #2A2A4E; color: #A0A0FF; }
QPushButton {
    background: #1A1A2A; border: 1px solid #2A2A4A;
    color: #8888AA; border-radius: 4px;
    padding: 4px 10px; font-size: 11px;
}
QPushButton:hover   { background: #2A2A4A; color: #C0C0FF; }
QPushButton:pressed { background: #0A0A1A; }
)";

ChapterWidget::ChapterWidget(QWidget* parent) : QWidget(parent)
{
    setStyleSheet(kStyle);
    setMinimumWidth(220);

    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    m_titleLabel = new QLabel("CHAPTERS", this);
    m_titleLabel->setObjectName("title");
    vl->addWidget(m_titleLabel);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(false);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    vl->addWidget(m_list, 1);

    auto* hl = new QHBoxLayout;
    hl->setContentsMargins(4, 4, 4, 4);
    hl->setSpacing(4);
    m_prevBtn = new QPushButton("◀ Prev", this);
    m_nextBtn = new QPushButton("Next ▶", this);
    hl->addWidget(m_prevBtn);
    hl->addWidget(m_nextBtn);
    vl->addLayout(hl);

    connect(m_list, &QListWidget::itemDoubleClicked,
            this,   &ChapterWidget::onItemDoubleClicked);
    connect(m_prevBtn, &QPushButton::clicked, this, &ChapterWidget::onPrev);
    connect(m_nextBtn, &QPushButton::clicked, this, &ChapterWidget::onNext);
}

void ChapterWidget::setChapters(const QVector<Chapter>& chapters, double totalDuration)
{
    m_chapters = chapters;
    m_duration = totalDuration;
    rebuild();
}

void ChapterWidget::setCurrentPosition(double seconds)
{
    m_position = seconds;
    highlightCurrentChapter();
}

void ChapterWidget::clear()
{
    m_chapters.clear();
    m_duration = 0.0;
    m_position = 0.0;
    m_list->clear();
}

int ChapterWidget::chapterIndexAt(double seconds) const
{
    int idx = -1;
    for (int i = 0; i < m_chapters.size(); ++i) {
        if (seconds >= m_chapters[i].startTime)
            idx = i;
    }
    return idx;
}

void ChapterWidget::rebuild()
{
    m_list->clear();
    for (int i = 0; i < m_chapters.size(); ++i) {
        const auto& ch  = m_chapters[i];
        double       end = (i + 1 < m_chapters.size())
                              ? m_chapters[i + 1].startTime
                              : m_duration;
        QString text = QString("%1.  %2\n       %3  –  %4")
                           .arg(i + 1)
                           .arg(ch.title)
                           .arg(formatTime(ch.startTime))
                           .arg(formatTime(end));
        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, ch.startTime);
        m_list->addItem(item);
    }
    highlightCurrentChapter();
}

void ChapterWidget::highlightCurrentChapter()
{
    if (m_list->count() == 0) return;
    int idx = chapterIndexAt(m_position);
    if (idx >= 0 && idx < m_list->count())
        m_list->setCurrentRow(idx);
}

QString ChapterWidget::formatTime(double secs) const
{
    int s = static_cast<int>(secs);
    int h = s / 3600; s %= 3600;
    int m = s / 60;   s %= 60;
    if (h > 0)
        return QString("%1:%2:%3")
                   .arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

void ChapterWidget::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;
    double t = item->data(Qt::UserRole).toDouble();
    emit chapterSelected(t);
}

void ChapterWidget::onPrev()
{
    int idx = chapterIndexAt(m_position);
    if (idx > 0) {
        double t = m_chapters[idx - 1].startTime;
        emit chapterSelected(t);
    } else if (!m_chapters.isEmpty()) {
        emit chapterSelected(0.0);
    }
}

void ChapterWidget::onNext()
{
    int idx = chapterIndexAt(m_position);
    if (idx >= 0 && idx + 1 < m_chapters.size()) {
        double t = m_chapters[idx + 1].startTime;
        emit chapterSelected(t);
    }
}
