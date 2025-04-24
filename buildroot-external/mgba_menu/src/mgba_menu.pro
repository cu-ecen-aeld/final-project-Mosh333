# Fix for: error: QApplication: No such file or directory
# Solution: add `QT += widgets` (from https://stackoverflow.com/questions/8995399/error-qapplication-no-such-file-or-directory)

QT       += widgets
CONFIG   += c++17
TARGET    = mgba_menu
TEMPLATE  = app
LIBS += -lSDL2

SOURCES  += main.cpp