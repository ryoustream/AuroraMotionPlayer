#pragma once
#include <QApplication>
class AuroraApplication : public QApplication {
    Q_OBJECT
public:
    AuroraApplication(int& argc, char** argv);
    bool notify(QObject* receiver, QEvent* event) override;
};
