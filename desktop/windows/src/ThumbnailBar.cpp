#include "ThumbnailBar.h"
ThumbnailBar::ThumbnailBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(48);
    setStyleSheet("background:#0A0A0F;");
    hide(); // Show only on hover over seek bar
}
