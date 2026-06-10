#include "AuroraApplication.h"
#include <QMessageBox>

AuroraApplication::AuroraApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setApplicationName("Aurora Motion Player");
    setApplicationVersion("1.0.0");
    setOrganizationName("Aurora");
}

bool AuroraApplication::notify(QObject* receiver, QEvent* event) {
    try {
        return QApplication::notify(receiver, event);
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "Aurora Error",
            QString("Unexpected error: %1").arg(e.what()));
        return false;
    } catch (...) {
        QMessageBox::critical(nullptr, "Aurora Error", "Unknown error occurred.");
        return false;
    }
}
