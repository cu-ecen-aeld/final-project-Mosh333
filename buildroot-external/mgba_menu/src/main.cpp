// Your latest working base + fixes applied directly without refactor

#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QFile>
#include <QDebug>
#include <QTimer>
#include <QSocketNotifier>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#include <SDL2/SDL.h>

bool isRaspberryPi()
{
    QFile f("/proc/device-tree/model");
    if (!f.open(QIODevice::ReadOnly)) return false;
    return f.readAll().contains("Raspberry Pi");
}

class MenuWindow : public QWidget
{
public:
    MenuWindow()
    {
        setWindowTitle("GBA UI Menu");
        setFixedSize(1920, 1080);

        if (SDL_Init(SDL_INIT_GAMECONTROLLER) == 0) {
            if (SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
                SDL_GameController *gc = SDL_GameControllerOpen(0);
                if (gc) SDL_GameControllerClose(gc);
            }
            SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        }

        stack_ = new QStackedWidget(this);
        QWidget *gridPage = buildMainGrid();
        stack_->addWidget(gridPage);

        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->addWidget(stack_);

        QTimer::singleShot(100, this, [this]() { updateFocus(); });

        openEvdevGamepad();
    }

    ~MenuWindow()
    {
        if (gamepadFd_ >= 0) ::close(gamepadFd_);
    }

private:
    enum MenuMode { MainMenu, SubMenu } mode_ = MainMenu;

    void openEvdevGamepad()
    {
        for (int i = 0; i < 32; ++i) {
            QString path = QString("/dev/input/event%1").arg(i);
            int fd = open(path.toStdString().c_str(), O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;
            char name[256] = "";
            ioctl(fd, EVIOCGNAME(sizeof(name)), name);
            if (QString(name).contains("8BitDo", Qt::CaseInsensitive)) {
                gamepadFd_ = fd;
                notifier_ = new QSocketNotifier(gamepadFd_, QSocketNotifier::Read, this);
                connect(notifier_, &QSocketNotifier::activated, this, &MenuWindow::handleInputEvent);
                return;
            }
            ::close(fd);
        }
    }

    void handleInputEvent()
    {
        struct input_event ev;
        while (read(gamepadFd_, &ev, sizeof(ev)) > 0) {
            if (ev.type == EV_KEY && ev.value == 1) {
                if (mode_ == MainMenu) handleMainInput(ev.code);
                else handleSubInput(ev.code);
            } else if (ev.type == EV_ABS) {
                if (mode_ == MainMenu) handleMainAxis(ev.code, ev.value);
                else handleSubAxis(ev.code, ev.value);
            }
        }
    }

    void handleMainInput(int code)
    {
        switch (code) {
        case 304: case 28: case 57:
            buttons_[currentRow_ * cols_ + currentCol_]->click();
            break;
        case 315:
            QApplication::quit();
            break;
        case 103:
            currentRow_ = (currentRow_ - 1 + rows_) % rows_;
            updateFocus();
            break;
        case 108:
            currentRow_ = (currentRow_ + 1) % rows_;
            updateFocus();
            break;
        case 105:
            currentCol_ = (currentCol_ - 1 + cols_) % cols_;
            updateFocus();
            break;
        case 106:
            currentCol_ = (currentCol_ + 1) % cols_;
            updateFocus();
            break;
        }
    }

    void handleMainAxis(int code, int value)
    {
        if (code == 16) {
            if (value == -1) currentCol_ = (currentCol_ - 1 + cols_) % cols_;
            else if (value == 1) currentCol_ = (currentCol_ + 1) % cols_;
            updateFocus();
        } else if (code == 17) {
            if (value == -1) currentRow_ = (currentRow_ - 1 + rows_) % rows_;
            else if (value == 1) currentRow_ = (currentRow_ + 1) % rows_;
            updateFocus();
        }
    }

    void handleSubInput(int code)
    {
        if (code == 14 || code == 305) {
            stack_->setCurrentIndex(0);
            mode_ = MainMenu;
            currentRow_ = 0;
            currentCol_ = 0;
            updateFocus();
            return;
        }

        if (code == 304 || code == 28 || code == 57) {
            if (!subButtons_.isEmpty())
                subButtons_[subFocusIndex_]->click();
        }

        if (code == 103) {
            subFocusIndex_ = (subFocusIndex_ - 1 + subButtons_.size()) % subButtons_.size();
            updateSubFocus();
        }
        else if (code == 108) {
            subFocusIndex_ = (subFocusIndex_ + 1) % subButtons_.size();
            updateSubFocus();
        }
    }

    void handleSubAxis(int code, int value)
    {
        if (code == 17) {
            if (value == -1) subFocusIndex_ = (subFocusIndex_ - 1 + subButtons_.size()) % subButtons_.size();
            else if (value == 1) subFocusIndex_ = (subFocusIndex_ + 1) % subButtons_.size();
            updateSubFocus();
        }
    }

    void updateFocus()
    {
        activateWindow();
        for (int i = 0; i < buttons_.size(); ++i) {
            if (i == currentRow_ * cols_ + currentCol_)
                buttons_[i]->setFocus(Qt::OtherFocusReason);
            else
                buttons_[i]->clearFocus();
        }
        qApp->processEvents();
    }

    void updateSubFocus()
    {
        for (int i = 0; i < subButtons_.size(); ++i) {
            if (i == subFocusIndex_)
                subButtons_[i]->setFocus(Qt::OtherFocusReason);
            else
                subButtons_[i]->clearFocus();
        }
        qApp->processEvents();
    }

    QWidget* buildMainGrid()
    {
        auto *page = new QWidget;
        auto *title = new QLabel("mGBA Launch Menu");
        title->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        title->setStyleSheet("font-size:48px;font-weight:bold;");
        title->setContentsMargins(0, 40, 0, 0);

        auto *grid = new QGridLayout;
        grid->setSpacing(10);
        grid->setContentsMargins(0, 0, 0, 0);

        QStringList names = { "Play", "Options", "About", "Download", "File Explorer", "System", "Settings", "Background", "Quit" };

        for (int i = 0; i < rows_ * cols_; ++i)
        {
            auto *btn = new QPushButton(names[i]);
            btn->setFixedSize(400, 100);
            btn->setStyleSheet(defaultBtnStyle);
            btn->setFocusPolicy(Qt::StrongFocus);
            connect(btn, &QPushButton::clicked, this, [this,i]{ onMainButton(i); });
            buttons_.append(btn);
            grid->addWidget(btn, i / cols_, i % cols_);
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
        subButtons_.clear();

        if (pages_.contains(titleText)) {
            QWidget *oldPage = pages_.take(titleText);
            stack_->removeWidget(oldPage);
            oldPage->deleteLater();
        }

        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(50, 50, 50, 50);
        layout->setSpacing(30);

        auto *topTitle = new QLabel(titleText);
        topTitle->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        topTitle->setStyleSheet("font-size:48px;font-weight:bold;");
        layout->addWidget(topTitle);

        auto *fakeBtn = new QPushButton(titleText);
        fakeBtn->setFixedSize(1000, 150);
        fakeBtn->setStyleSheet(defaultBtnStyle);
        fakeBtn->setFocusPolicy(Qt::StrongFocus);

        // Customized dummy handler
        connect(fakeBtn, &QPushButton::clicked, this, [this, titleText] {
            if (titleText == "Play") {
                qDebug() << "[action] Play button clicked!";
                // TODO: Launch Play menu
            }
            else if (titleText == "Options") {
                qDebug() << "[action] Options button clicked!";
                // TODO: Open Options menu
            }
            else if (titleText == "About") {
                qDebug() << "[action] About button clicked!";
                // TODO: Show About information
            }
            else if (titleText == "Download") {
                qDebug() << "[action] Download button clicked!";
                // TODO: Launch Download Manager
            }
            else if (titleText == "File Explorer") {
                qDebug() << "[action] File Explorer button clicked!";
                // TODO: Open File Explorer
            }
            else if (titleText == "System") {
                qDebug() << "[action] System button clicked!";
                // TODO: Show System Settings
            }
            else if (titleText == "Settings") {
                qDebug() << "[action] Settings button clicked!";
                // TODO: Open General Settings
            }
            else if (titleText == "Background") {
                qDebug() << "[action] Background button clicked!";
                // TODO: Set or Change Background
            }
            else if (titleText == "Quit") {
                qDebug() << "[action] Quit button clicked!";
                // TODO: Maybe confirm quitting?
            }
            else {
                qDebug() << "[dummy] Unknown button clicked:" << titleText;
            }
        });

        auto *backBtn = new QPushButton("Back to Main Menu");
        backBtn->setFixedSize(1000, 150);
        backBtn->setStyleSheet(defaultBtnStyle);
        backBtn->setFocusPolicy(Qt::StrongFocus);
        connect(backBtn, &QPushButton::clicked, this, [this]{
            stack_->setCurrentIndex(0);
            mode_ = MainMenu;
            currentRow_ = 0;
            currentCol_ = 0;
            updateFocus();
        });

        layout->addWidget(fakeBtn, 0, Qt::AlignHCenter);
        layout->addWidget(backBtn, 0, Qt::AlignHCenter);
        layout->addStretch();

        subButtons_.append(fakeBtn);
        subButtons_.append(backBtn);

        pages_.insert(titleText, page);
        stack_->addWidget(page);

        // --- REAL fix starts here ---
        stack_->setCurrentWidget(page);   // <--- ADD this line
        subFocusIndex_ = 0;
        mode_ = SubMenu;
        updateSubFocus();
        activateWindow();
        qApp->processEvents();
        // --- REAL fix ends here ---

        return page;
    }

    void onMainButton(int idx)
    {
        switch (idx)
        {
        case 0: handlePlay(); break;
        case 1: stack_->setCurrentWidget(pageFor("Options")); break;
        case 2: stack_->setCurrentWidget(pageFor("About")); break;
        case 3: stack_->setCurrentWidget(pageFor("Download")); break;
        case 4: stack_->setCurrentWidget(pageFor("File Explorer")); break;
        case 5: stack_->setCurrentWidget(pageFor("System")); break;
        case 6: stack_->setCurrentWidget(pageFor("Settings")); break;
        case 7: stack_->setCurrentWidget(pageFor("Background")); break;
        case 8: stack_->setCurrentWidget(pageFor("Quit")); break;
        }
    }

    void handlePlay()
    {
        if (isRaspberryPi()) {
            ::execl("/usr/bin/mgba-qt", "mgba-qt", "-b", "/root/gba_bios.bin", "/root/mgba_rom_files/Megaman_Battle_Network_4_Blue_Moon_USA/megaman_bn4.gba", static_cast<char*>(nullptr));
            QApplication::exit(1);
        } else {
            QApplication::quit();
        }
    }

    QStackedWidget *stack_ = nullptr;
    QVector<QPushButton*> buttons_;
    QVector<QPushButton*> subButtons_;
    QHash<QString, QWidget*> pages_;
    int currentRow_ = 0, currentCol_ = 0;
    int subFocusIndex_ = 0;
    const int rows_ = 3, cols_ = 3;
    int gamepadFd_ = -1;
    QSocketNotifier *notifier_ = nullptr;

    const QString defaultBtnStyle =
        "QPushButton { background: rgb(100,149,237); color: white; font-size: 24px; border-radius: 8px; }"
        "QPushButton:hover { background: rgb(65,105,225); }"
        "QPushButton:focus { background: rgb(65,105,225); border: 4px solid yellow; }"
        "QPushButton:pressed { background: rgb(40,90,200); }";
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MenuWindow w;
    w.show();
    return app.exec();
}
