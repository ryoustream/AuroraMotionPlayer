#pragma once
#include <QWidget>
class ThumbnailBar : public QWidget {
    Q_OBJECT
public:
    explicit ThumbnailBar(QWidget* parent = nullptr);
signals:
    void seekRequested(double seconds);
};
