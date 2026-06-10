#pragma once
#include <QWidget>
#include <QString>
class SubtitleOverlay : public QWidget {
    Q_OBJECT
public:
    explicit SubtitleOverlay(QWidget* parent = nullptr);
    void setText(const QString& text);
    void setStyle(const QString& font, int size, QColor color, QColor outline);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_text;
    QString m_font  = "Arial";
    int     m_size  = 28;
    QColor  m_color = Qt::white;
    QColor  m_outline = Qt::black;
};
