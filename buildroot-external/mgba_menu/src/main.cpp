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
#include <QDir>
#include <QFileInfo>
#include <QScrollArea>
#include <QSocketNotifier>
#include <QProcess>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <QImageReader>
#include <QDebug>
#include <QMessageBox>

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
        setObjectName("MenuWindow");
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
    QScrollArea *romScrollArea_ = nullptr;
    QWidget *romScrollWidget_ = nullptr;
    
    QWidget* mainMenuWrapper_ = nullptr;  // Add this to keep a reference
    QString currentBackground_; // New: empty = default light grey background Fusion Style: #f0f0f0 → RGB(240, 240, 240)
    QScrollArea *bgScrollArea_ = nullptr;
    QWidget *bgScrollWidget_ = nullptr;

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
            if (i == subFocusIndex_) {
                subButtons_[i]->setFocus(Qt::OtherFocusReason);

                // --- NEW: Ensure button is visible inside scroll area ---
                if (romScrollArea_ && romScrollArea_->isVisible()) {
                    romScrollArea_->ensureWidgetVisible(subButtons_[i]);
                }
                else if (bgScrollArea_ && bgScrollArea_->isVisible()) {
                    bgScrollArea_->ensureWidgetVisible(subButtons_[i]);
                }                
            }
            else {
                subButtons_[i]->clearFocus();
            }
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
        mainMenuWrapper_ = buildBackgroundWrapped(page, currentBackground_);
        return mainMenuWrapper_;
    }

    QWidget* buildBackgroundWrapped(QWidget* innerContent, const QString& imagePath)
    {
        QWidget *wrapper = new QWidget;

        if (!imagePath.isEmpty()) {
            wrapper->setStyleSheet(QString(
                "QWidget { "
                "background-image: url(%1); "
                "background-repeat: no-repeat; "
                "background-position: center; "
                "}").arg(imagePath));
        }

        QVBoxLayout *v = new QVBoxLayout(wrapper);
        v->setContentsMargins(0, 0, 0, 0);
        v->addWidget(innerContent);

        return wrapper;
    }

    QStringList findBackgroundImages()
    {
        QDir bgDir("/root/background_image");
        QStringList filters = { "*.bmp" };
        QFileInfoList files = bgDir.entryInfoList(filters, QDir::Files);
        QStringList result;
        for (const QFileInfo &file : files)
            result << file.absoluteFilePath();
        return result;
    }

    void showBackgroundSelector()
    {
        // Add this line to check supported formats
        qDebug() << "[Qt image formats]" << QImageReader::supportedImageFormats();

        subButtons_.clear();

        if (pages_.contains("Background")) {
            QWidget *oldPage = pages_.take("Background");
            stack_->removeWidget(oldPage);
            oldPage->deleteLater();
            // --- Reset broken references ---
            bgScrollArea_ = nullptr;
            bgScrollWidget_ = nullptr;
        }

        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(50, 50, 50, 50);
        layout->setSpacing(20);

        auto *title = new QLabel("Choose a Background");
        title->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        title->setStyleSheet("font-size:48px;font-weight:bold;");
        layout->addWidget(title);

        bgScrollArea_ = new QScrollArea;
        bgScrollArea_->setWidgetResizable(true);
        bgScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        bgScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        bgScrollWidget_ = new QWidget;
        QVBoxLayout *innerLayout = new QVBoxLayout(bgScrollWidget_);
        innerLayout->setSpacing(10);

        QStringList images = findBackgroundImages();
        for (const QString &imgPath : images) {
            QString displayName = QFileInfo(imgPath).fileName();
            QPushButton *btn = new QPushButton(displayName);
            btn->setFixedSize(1000, 100);
            btn->setStyleSheet(defaultBtnStyle);
            btn->setFocusPolicy(Qt::StrongFocus);

            connect(btn, &QPushButton::clicked, this, [this, imgPath] {
                currentBackground_ = imgPath;
                qDebug() << "[background] Changed to:" << imgPath;

                currentBackground_ = imgPath;
                if (mainMenuWrapper_) {
                    stack_->setStyleSheet(QString(
                        "QStackedWidget { "
                        "background-image: url(%1); "
                        "background-repeat: no-repeat; "
                        "background-position: center; "
                        "}").arg(currentBackground_));
                }
                stack_->setCurrentIndex(0);
                mode_ = MainMenu;
                currentRow_ = 0;
                currentCol_ = 0;
                updateFocus();

            });

            innerLayout->addWidget(btn, 0, Qt::AlignHCenter);
            subButtons_.append(btn);
        }

        // Reset background button
        QPushButton *resetBtn = new QPushButton("Reset to Default");
        resetBtn->setFixedSize(1000, 100);
        resetBtn->setStyleSheet(defaultBtnStyle);
        resetBtn->setFocusPolicy(Qt::StrongFocus);
        connect(resetBtn, &QPushButton::clicked, this, [this] {
            currentBackground_.clear();
            if (mainMenuWrapper_) {
                stack_->setStyleSheet("");  // <-- Fix: Apply empty stylesheet to the stack_
            }
            stack_->setCurrentIndex(0);
            mode_ = MainMenu;
            currentRow_ = 0;
            currentCol_ = 0;
            updateFocus();
            bgScrollArea_ = nullptr;
            bgScrollWidget_ = nullptr;
        });

        innerLayout->addWidget(resetBtn, 0, Qt::AlignHCenter);
        subButtons_.append(resetBtn);

        bgScrollWidget_->setLayout(innerLayout);
        bgScrollArea_->setWidget(bgScrollWidget_);
        layout->addWidget(bgScrollArea_, 1);

        QPushButton *backBtn = new QPushButton("Back to Main Menu");
        backBtn->setFixedSize(1000, 100);
        backBtn->setStyleSheet(defaultBtnStyle);
        backBtn->setFocusPolicy(Qt::StrongFocus);
        connect(backBtn, &QPushButton::clicked, this, [this] {
            stack_->setCurrentIndex(0);
            mode_ = MainMenu;
            currentRow_ = 0;
            currentCol_ = 0;
            updateFocus();
        });

        layout->addWidget(backBtn, 0, Qt::AlignHCenter);
        subButtons_.append(backBtn);

        pages_.insert("Background", page);
        stack_->addWidget(page);
        stack_->setCurrentWidget(page);
        subFocusIndex_ = 0;
        mode_ = SubMenu;
        updateSubFocus();
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
        connect(fakeBtn, &QPushButton::clicked, this, [this, titleText, fakeBtn] {
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
                fakeBtn->setText("Downloading...");
                fakeBtn->setEnabled(false);

                QProcess *proc = new QProcess(this);
                connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [fakeBtn](int /*exitCode*/, QProcess::ExitStatus /*status*/) {
                    fakeBtn->setText("Downloaded");
                    fakeBtn->setEnabled(true);
                });

                proc->start("/root/download_roms.sh");
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
                qDebug() << "[action] Power Off button clicked!";
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this, "Confirm Shutdown", "Are you sure you want to power off?",
                                            QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    QProcess::execute("poweroff");
                }
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
        case 7: showBackgroundSelector(); break;
        case 8: stack_->setCurrentWidget(pageFor("Quit")); break;
        }
    }

    void handlePlay()
    {
        if (isRaspberryPi()) {
            // ::execl("/usr/bin/mgba-qt", "mgba-qt", "-b", "/root/gba_bios.bin", "/root/mgba_rom_files/Megaman_Battle_Network_4_Blue_Moon_USA/megaman_bn4.gba", static_cast<char*>(nullptr));
            // QApplication::exit(1);
            showRomSelector();
        } else {
            QApplication::quit();
        }
    }

    void showRomSelector()
    {
        subButtons_.clear();

        if (pages_.contains("Play")) {
            QWidget *oldPage = pages_.take("Play");
            stack_->removeWidget(oldPage);
            oldPage->deleteLater();
            romScrollArea_ = nullptr;
            romScrollWidget_ = nullptr;
        }

        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(50, 50, 50, 50);
        layout->setSpacing(20);

        auto *title = new QLabel("Select a Game");
        title->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        title->setStyleSheet("font-size:48px;font-weight:bold;");
        layout->addWidget(title);

        // --- NEW: Scroll area for ROM buttons ---
        romScrollArea_ = new QScrollArea;
        romScrollArea_->setWidgetResizable(true);
        romScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        romScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        romScrollWidget_ = new QWidget;
        auto *romLayout = new QVBoxLayout(romScrollWidget_);
        romLayout->setContentsMargins(0, 0, 0, 0);
        romLayout->setSpacing(10);

        QStringList romPaths = findRomFiles();

        for (const QString &romPath : romPaths) {
            QString displayName = QFileInfo(romPath).completeBaseName().replace("_", " "); // prettier name
            auto *btn = new QPushButton(displayName);
            btn->setFixedSize(1000, 100);
            btn->setStyleSheet(defaultBtnStyle);
            btn->setFocusPolicy(Qt::StrongFocus);

            connect(btn, &QPushButton::clicked, this, [this, romPath] {
                launchRom(romPath);
            });

            romLayout->addWidget(btn, 0, Qt::AlignHCenter);
            subButtons_.append(btn);
        }

        romScrollWidget_->setLayout(romLayout);
        romScrollArea_->setWidget(romScrollWidget_);
        layout->addWidget(romScrollArea_, 1); // Take remaining space

        // Back button
        auto *backBtn = new QPushButton("Back to Main Menu");
        backBtn->setFixedSize(1000, 100);
        backBtn->setStyleSheet(defaultBtnStyle);
        backBtn->setFocusPolicy(Qt::StrongFocus);

        connect(backBtn, &QPushButton::clicked, this, [this]{
            stack_->setCurrentIndex(0);
            mode_ = MainMenu;
            currentRow_ = 0;
            currentCol_ = 0;
            updateFocus();
            romScrollArea_ = nullptr;
            romScrollWidget_ = nullptr;
        });

        layout->addWidget(backBtn, 0, Qt::AlignHCenter);
        subButtons_.append(backBtn);

        pages_.insert("Play", page);
        stack_->addWidget(page);

        stack_->setCurrentWidget(page);
        subFocusIndex_ = 0;
        mode_ = SubMenu;
        updateSubFocus();
    }


    QStringList findRomFiles()
    {
        QStringList result;
        QDir rootDir("/root/mgba_rom_files/");
        QFileInfoList dirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QFileInfo &folder : dirs) {
            QDir gameFolder(folder.absoluteFilePath());
            QFileInfoList roms = gameFolder.entryInfoList(QStringList() << "*.gba", QDir::Files);
            if (!roms.isEmpty()) {
                result.append(roms.first().absoluteFilePath());
            }
        }

        return result;
    }

    void launchRom(const QString &romPath)
    {
        if (isRaspberryPi()) {
            ::execl("/usr/bin/mgba-qt", "mgba-qt", "-b", "/root/gba_bios.bin", romPath.toUtf8().constData(), static_cast<char*>(nullptr));
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