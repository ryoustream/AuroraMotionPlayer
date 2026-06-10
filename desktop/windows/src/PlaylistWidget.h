#pragma once
#include <QWidget>
class QListWidget;
class PlaylistWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlaylistWidget(QWidget* parent = nullptr);
    void addItem(const QString& path);
    void clear();
signals:
    void itemActivated(const QString& path);
private:
    QListWidget* m_list = nullptr;
};
