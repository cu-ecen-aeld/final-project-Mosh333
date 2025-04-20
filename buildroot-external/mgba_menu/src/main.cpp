#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QFile>
#include <QDebug>
#include <vector>
#include <QHash>
#include <unistd.h>          // execl()

/* ---------- helpers ------------------------------------------------------ */
bool isRaspberryPi()
{
    QFile f("/proc/device-tree/model");
    if (!f.open(QIODevice::ReadOnly)) return false;
    return f.readAll().contains("Raspberry Pi");
}
/* ------------------------------------------------------------------------ */

class MenuWindow : public QWidget
{
public:
    MenuWindow()
    {
        setWindowTitle("GBA UI Menu");
        setFixedSize(1920, 1080);

        stack_ = new QStackedWidget(this);

        QWidget *gridPage = buildMainGrid();
        stack_->addWidget(gridPage);               // page 0

        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->addWidget(stack_);
    }

private:
    QWidget* buildMainGrid()
    {
        auto *page  = new QWidget;
        auto *title = new QLabel("mGBA Launch Menu");
        title->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        title->setStyleSheet("font-size:48px;font-weight:bold;");
        title->setContentsMargins(0, 40, 0, 0);

        auto *grid = new QGridLayout;
        grid->setSpacing(10);
        grid->setContentsMargins(0, 0, 0, 0);

        const QString btnStyle =
            "QPushButton {"
            "  background: rgb(100,149,237);"
            "  color: white;"
            "  font-size: 24px;"
            "  border-radius: 8px;"
            "}"
            "QPushButton:hover {"
            "  background: rgb(65,105,225);"
            "}"
            "QPushButton:pressed {"
            "  background: rgb(40,90,200);"
            "}";

        QStringList names = { "Play", "Options", "About",
                              "Save", "Load", "Scores",
                              "Credits", "Help", "Quit" };

        const int rows = 3, cols = 3;
        for (int i = 0; i < rows * cols; ++i)
        {
            auto *btn = new QPushButton(names[i]);
            btn->setFixedSize(400, 100);
            btn->setStyleSheet(btnStyle);
            connect(btn, &QPushButton::clicked,
                    this, [this,i]{ onMainButton(i); });

            grid->addWidget(btn, i / cols, i % cols);
        }

        auto *v = new QVBoxLayout(page);
        v->setContentsMargins(0, 30, 0, 0);
        v->addWidget(title);
        v->addSpacing(300);
        v->addLayout(grid, 1);
        v->addStretch();
        return page;
    }

    QWidget* pageFor(const QString &titleText)
    {
        if (pages_.contains(titleText))
            return pages_[titleText];

        auto *page = new QWidget;
        auto *back = new QPushButton("Back to Main Menu");
        back->setFixedSize(600, 200);
        back->setStyleSheet(
            "QPushButton {"
            "  font-size: 36px;"
            "  background: rgb(90,130,255);"
            "  color: white;"
            "  border-radius: 12px;"
            "}"
            "QPushButton:hover {"
            "  background: rgb(65,105,225);"
            "}"
            "QPushButton:pressed {"
            "  background: rgb(40,90,200);"
            "}");

        connect(back, &QPushButton::clicked,
                this, [this]{ stack_->setCurrentIndex(0); });

        auto *title = new QLabel(titleText);
        title->setAlignment(Qt::AlignHCenter);
        title->setStyleSheet("font-size:60px;font-weight:bold;");

        auto *lay = new QVBoxLayout(page);
        lay->addStretch();
        lay->addWidget(title, 0, Qt::AlignHCenter);
        lay->addSpacing(80);
        lay->addWidget(back, 0, Qt::AlignHCenter);
        lay->addStretch();

        pages_.insert(titleText, page);
        stack_->addWidget(page);
        return page;
    }

    void onMainButton(int idx)
    {
        switch (idx)
        {
        case 0:   handlePlay();                          break;
        case 1:   stack_->setCurrentWidget(pageFor("Options"));  break;
        default:  stack_->setCurrentWidget(pageFor(QString("Page %1").arg(idx)));
                  break;
        }
    }

    void handlePlay()
    {
        if (isRaspberryPi())
        {
            ::execl("/usr/bin/mgba-qt", "mgba-qt",
                    "-b", "/root/gba_bios.bin",
                    "/root/mgba_rom_files/"
                    "Megaman_Battle_Network_4_Blue_Moon_USA/megaman_bn4.gba",
                    static_cast<char*>(nullptr));
            qCritical() << "Failed to launch mgba‑qt!";
            QApplication::exit(1);
        }
        else
        {
            qDebug() << "[desktop] would launch game";
            QApplication::quit();
        }
    }

    QStackedWidget   *stack_  = nullptr;
    QHash<QString,QWidget*> pages_;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MenuWindow w;
    w.show();
    return app.exec();
}
