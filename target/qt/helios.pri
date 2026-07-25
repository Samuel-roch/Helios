#-------------------------------------------------------------------------------
# helios.pri — Helios SDK integration for Qt desktop simulation projects.
#
# Usage — add a single line to your .pro file:
#
#     include(<path-to>/helios/target/qt/helios.pri)
#
# What it does:
#   - Defines HeliosQt=1, which makes <hel_target> pull in target/qt/qt.hpp
#   - Puts the SDK root and include/ on INCLUDEPATH so <hel_*> resolves
#   - Lists every SDK header in the project tree and compiles the syslib sources
#
# Optional — set before the include() line:
#
#     CONFIG += helios_tests   # also compile the GoogleTest suites
#
# Requires: C++17 and the Qt SerialPort module.
#-------------------------------------------------------------------------------

isEmpty(HELIOS_PRI_INCLUDED) {
    HELIOS_PRI_INCLUDED = 1

    HELIOS_ROOT = $$clean_path($$PWD/../..)

    !exists($$HELIOS_ROOT/include/hel_target) {
        error("helios.pri: Helios SDK root not found at $$HELIOS_ROOT")
    }

    !qtHaveModule(serialport) {
        error("helios.pri: target/qt/qt.hpp needs the Qt SerialPort module — install qtserialport or add it to your kit")
    }

    CONFIG  += c++17
    QT      += serialport
    DEFINES += HeliosQt=1

    INCLUDEPATH += \
        $$HELIOS_ROOT \
        $$HELIOS_ROOT/include

    DEPENDPATH += $$HELIOS_ROOT

    # ---------------------------------------------------------------------------
    # Headers
    # ---------------------------------------------------------------------------

    HEADERS += \
        $$HELIOS_ROOT/hel_config.hpp \
        $$files($$HELIOS_ROOT/api/*.hpp, true) \
        $$files($$HELIOS_ROOT/syslib/*.hpp, true) \
        $$files($$HELIOS_ROOT/utils/*.hpp, true) \
        $$files($$HELIOS_ROOT/target/*.hpp, true)

    # The <hel_*> convenience headers carry no file extension; listing them here
    # instead of in HEADERS keeps them visible in the tree without feeding moc
    # a file name it cannot classify.
    OTHER_FILES += $$files($$HELIOS_ROOT/include/*)

    # ---------------------------------------------------------------------------
    # Sources
    # ---------------------------------------------------------------------------

    SOURCES += $$HELIOS_ROOT/syslib/pit/pit.cpp

    helios_tests {
        # Every suite is wrapped in __has_include("gtest/gtest.h") and reduces to
        # an empty translation unit when GoogleTest is not on the include path.
        SOURCES += \
            $$files($$HELIOS_ROOT/syslib/*_test.cpp, true) \
            $$files($$HELIOS_ROOT/utils/*_test.cpp, true)
    }

    # ---------------------------------------------------------------------------
    # Qt target implementations — libraries/rtos (iKernel/iMutex/iQueue/...) and
    # drivers (iUart/...) live in the consuming Qt/ project, not in the SDK
    # submodule itself. Pulled in here, rather than listed in Qt.pro, since they
    # are exactly what makes <hel_*> usable on this target. Guarded with exists()
    # — a no-op if this SDK is ever consumed from a project without a sibling
    # Qt/ folder in this layout.
    # ---------------------------------------------------------------------------

    QT_TARGET_ROOT = $$clean_path($$PWD/../../../Qt)

    exists($$QT_TARGET_ROOT/libraries/rtos) {
        INCLUDEPATH += $$QT_TARGET_ROOT/libraries/rtos
        SOURCES      += $$files($$QT_TARGET_ROOT/libraries/rtos/*.cpp, true)
        HEADERS      += $$files($$QT_TARGET_ROOT/libraries/rtos/*.hpp, true)
    }

    exists($$QT_TARGET_ROOT/drivers) {
        INCLUDEPATH += $$QT_TARGET_ROOT/drivers
        SOURCES      += $$files($$QT_TARGET_ROOT/drivers/*.cpp, true)
        HEADERS      += $$files($$QT_TARGET_ROOT/drivers/*.hpp, true)
    }
}
