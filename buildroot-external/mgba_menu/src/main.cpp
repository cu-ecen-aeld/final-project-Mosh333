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
#include <QTimer>
#include <QSocketNotifier>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#include <SDL2/SDL.h>


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
        qDebug() << "[init] Constructing MenuWindow...";
        setWindowTitle("GBA UI Menu");
        setFixedSize(1920, 1080);

        // --- Wake controller via SDL2 ---
        qDebug() << "[sdl2-wake] Initializing SDL2...";
        if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
            qWarning() << "[sdl2-wake] SDL_Init failed:" << SDL_GetError();
        } else {
            if (SDL_NumJoysticks() > 0) {
                qDebug() << "[sdl2-wake] Detected" << SDL_NumJoysticks() << "joystick(s)";
                if (SDL_IsGameController(0)) {
                    SDL_GameController *gc = SDL_GameControllerOpen(0);
                    if (gc) {
                        qDebug() << "[sdl2-wake] GameController opened!";
                        SDL_GameControllerClose(gc);
                        qDebug() << "[sdl2-wake] GameController closed (wake-up sent)";
                    } else {
                        qWarning() << "[sdl2-wake] GameControllerOpen failed:" << SDL_GetError();
                    }
                } else {
                    qWarning() << "[sdl2-wake] Joystick 0 is not a recognized game controller";
                }
            } else {
                qWarning() << "[sdl2-wake] No joysticks found";
            }
            SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        }

        stack_ = new QStackedWidget(this);

        QWidget *gridPage = buildMainGrid();
        stack_->addWidget(gridPage);

        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->addWidget(stack_);

        QTimer::singleShot(0, this, [this]() {
            qDebug() << "[init] Running updateFocus after layout...";
            updateFocus();
        });

        openEvdevGamepad();
    }

    ~MenuWindow()
    {
        qDebug() << "[shutdown] Closing gamepad file descriptor...";
        if (gamepadFd_ >= 0) ::close(gamepadFd_);
    }

private:
    void openEvdevGamepad()
    {
        qDebug() << "[evdev] Scanning for input devices...";
        for (int i = 0; i < 32; ++i) {
            QString path = QString("/dev/input/event%1").arg(i);
            int fd = open(path.toStdString().c_str(), O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;

            char name[256] = "";
            ioctl(fd, EVIOCGNAME(sizeof(name)), name);
            qDebug() << "[evdev] Found" << path << "->" << name;

            if (QString(name).contains("8BitDo", Qt::CaseInsensitive)) {
                qDebug() << "[evdev] Matched gamepad device at" << path;
                gamepadFd_ = fd;
                notifier_ = new QSocketNotifier(gamepadFd_, QSocketNotifier::Read, this);
                connect(notifier_, &QSocketNotifier::activated, this, &MenuWindow::handleInputEvent);
                break;
            }
            ::close(fd);
        }

        if (gamepadFd_ < 0) {
            qWarning() << "[evdev] No gamepad found.";
        }
    }

    void handleInputEvent()
    {
        //use evtest /dev/input/eventX on pi to figure out for given hardware device
        struct input_event ev;
        while (read(gamepadFd_, &ev, sizeof(ev)) > 0) {
            qDebug() << "[evdev] event received - type:" << ev.type << "code:" << ev.code << "value:" << ev.value;

            // -------- BUTTON PRESSES (controller or keyboard) ----------
            if (ev.type == EV_KEY && ev.value == 1) {
                switch (ev.code) {
                    case 304:  // Gamepad A
                    case 28:   // Keyboard Enter
                    case 57:   // Keyboard Space
                        qDebug() << "[evdev] A/Enter/Space pressed - clicking button";
                        buttons_[currentRow_ * cols_ + currentCol_]->click();
                        break;
                    case 305:
                        qDebug() << "[evdev] B button pressed (no action)";
                        break;
                    case 315:
                        qDebug() << "[evdev] Start button pressed - exiting...";
                        QApplication::quit();
                        break;
                    case 14:   // Keyboard Backspace
                        qDebug() << "[evdev] Backspace - going to main menu";
                        stack_->setCurrentIndex(0);
                        break;
                    case 103:  // Keyboard Up
                        qDebug() << "[evdev] KEY_UP";
                        currentRow_ = (currentRow_ - 1 + rows_) % rows_;
                        updateFocus();
                        break;
                    case 108:  // Keyboard Down
                        qDebug() << "[evdev] KEY_DOWN";
                        currentRow_ = (currentRow_ + 1) % rows_;
                        updateFocus();
                        break;
                    case 105:  // Keyboard Left
                        qDebug() << "[evdev] KEY_LEFT";
                        currentCol_ = (currentCol_ - 1 + cols_) % cols_;
                        updateFocus();
                        break;
                    case 106:  // Keyboard Right
                        qDebug() << "[evdev] KEY_RIGHT";
                        currentCol_ = (currentCol_ + 1) % cols_;
                        updateFocus();
                        break;
                    default:
                        qDebug() << "[evdev] Unhandled KEY code:" << ev.code;
                        break;
                }

            // -------- D-PAD / JOYSTICK (controller only) ----------
            } else if (ev.type == EV_ABS) {
                if (ev.code == 16) { // D-pad Left/Right
                    if (ev.value == -1) {
                        qDebug() << "[evdev] DPAD LEFT";
                        currentCol_ = (currentCol_ - 1 + cols_) % cols_;
                        updateFocus();
                    } else if (ev.value == 1) {
                        qDebug() << "[evdev] DPAD RIGHT";
                        currentCol_ = (currentCol_ + 1) % cols_;
                        updateFocus();
                    }
                } else if (ev.code == 17) { // D-pad Up/Down
                    if (ev.value == -1) {
                        qDebug() << "[evdev] DPAD UP";
                        currentRow_ = (currentRow_ - 1 + rows_) % rows_;
                        updateFocus();
                    } else if (ev.value == 1) {
                        qDebug() << "[evdev] DPAD DOWN";
                        currentRow_ = (currentRow_ + 1) % rows_;
                        updateFocus();
                    }
                } else {
                    qDebug() << "[evdev] Unhandled ABS code:" << ev.code;
                }
            } else {
                qDebug() << "[evdev] Ignored event type:" << ev.type;
            }
        }
    }


    void updateFocus()
    {
        qDebug() << "[focus] Updating to row" << currentRow_ << "col" << currentCol_;
        for (int i = 0; i < buttons_.size(); ++i) {
            if (i == currentRow_ * cols_ + currentCol_) {
                qDebug() << "[focus] Highlighting button index" << i;
                // buttons_[i]->setStyleSheet(defaultBtnStyle + "border: 4px solid yellow;");
                buttons_[i]->setFocus();
            } else {
                buttons_[i]->setStyleSheet(defaultBtnStyle);
            }
        }
    }

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

        QStringList names = { "Play", "Options", "About",
                              "Save", "Load", "Scores",
                              "Credits", "Help", "Quit" };

        const int rows = 3, cols = 3;
        for (int i = 0; i < rows * cols; ++i)
        {
            auto *btn = new QPushButton(names[i]);
            btn->setFixedSize(400, 100);
            btn->setStyleSheet(defaultBtnStyle);
            btn->setFocusPolicy(Qt::StrongFocus);
            connect(btn, &QPushButton::clicked,
                    this, [this,i]{ onMainButton(i); });

            buttons_.append(btn);
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

        qDebug() << "[page] Creating page for" << titleText;

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
        qDebug() << "[action] Button" << idx << "clicked.";
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
        qDebug() << "[launch] handlePlay() called.";
        if (isRaspberryPi())
        {
            qDebug() << "[launch] Running mgba-qt on Raspberry Pi...";
            ::execl("/usr/bin/mgba-qt", "mgba-qt",
                    "-b", "/root/gba_bios.bin",
                    "/root/mgba_rom_files/Megaman_Battle_Network_4_Blue_Moon_USA/megaman_bn4.gba",
                    static_cast<char*>(nullptr));
            qCritical() << "Failed to launch mgba‑qt!";
            QApplication::exit(1);
        }
        else
        {
            qDebug() << "[launch] Desktop mode - quitting app.";
            QApplication::quit();
        }
    }

    QStackedWidget   *stack_  = nullptr;
    QHash<QString,QWidget*> pages_;
    QVector<QPushButton*> buttons_;
    int currentRow_ = 0, currentCol_ = 0;
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
    qDebug() << "[main] Starting application...";
    QApplication app(argc, argv);
    MenuWindow w;
    w.show();
    return app.exec();
}
