#include "calculate.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("计算器"));
    app.setApplicationDisplayName(QStringLiteral("计算器"));

    Calculate window;
    window.show();

    return app.exec();
}
