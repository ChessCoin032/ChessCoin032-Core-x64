TEMPLATE = app
TARGET = chesscoin-qt
INCLUDEPATH += src src/json src/qt
DEFINES += QT_GUI BOOST_THREAD_USE_LIB BOOST_SPIRIT_THREADSAFE QT_SUPPORTSSL TWO_CHAT_VERSIONS SECP256K1_STATIC SQLITE_THREADSAFE=1 __STDC_FORMAT_MACROS __STDC_LIMIT_MACROS
!win32:DEFINES += WAYLANDMODE
CONFIG += no_include_pwd
CONFIG += thread
win32:CONFIG -= embed_manifest_exe multimedia-wmf
CONFIG += static release
CONFIG += staticlib

QT += core gui network multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += widgets
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
    DEFINES += QT_DEPRECATED_WARNINGS
}

# Optimization level change (e.g., avoid aggressive optimizations)
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE -= -O1

# Ensure no potentially unsafe optimization flags are used
QMAKE_CXXFLAGS -= -fno-omit-frame-pointer

QMAKE_CXXFLAGS += -std=c++14
QMAKE_CXXFLAGS += -fPIC
macx: QMAKE_CXXFLAGS += -Wno-enum-constexpr-conversion

# for boost 1.77, add -mt to the boost libraries
# use: qmake BOOST_LIB_SUFFIX=-mt
# for boost thread win32 with _win32 sufix
# use: BOOST_THREAD_LIB_SUFFIX=_win32-...
# or when linking against a specific BerkelyDB version: BDB_LIB_SUFFIX=-4.8

# Dependency library locations can be customized with:
#    BOOST_INCLUDE_PATH, BOOST_LIB_PATH, BDB_INCLUDE_PATH,
#    BDB_LIB_PATH, OPENSSL_INCLUDE_PATH and OPENSSL_LIB_PATH respectively

OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build

BASE_DIR=C:/Projects/src/ChessCoin032-Core-x64-dev/3rd-party
#BASE_DIR=/home/aaa/ChessCoin032-Core-x64/3rd-party

win32:BOOST_LIB_SUFFIX=-mgw8-mt-s-x64-1_77
macx {
    BOOST_LIB_SUFFIX=-mt-s-a64
	
	HEADERS += src/qt/macdockiconhandler.h
	OBJECTIVE_SOURCES += src/qt/macdockiconhandler.mm
	LIBS += -framework Foundation -framework ApplicationServices -framework AppKit
	DEFINES += MAC_OSX MSG_NOSIGNAL=0
	ICON = src/qt/res/icons/bitcoin.icns
	TARGET = "chesscoin-qt"
	QMAKE_CFLAGS_THREAD += -pthread
	QMAKE_LFLAGS_THREAD += -pthread
	QMAKE_CXXFLAGS_THREAD += -pthread
} else:!win32 {
    contains(QT_ARCH, arm64) {
        BOOST_LIB_SUFFIX=-mt-s-a64
    } else {
        BOOST_LIB_SUFFIX=-mt-s-x64
    }
}
BOOST_INCLUDE_PATH=$$BASE_DIR/boost_1_77_0
BOOST_LIB_PATH=$$BASE_DIR/boost_1_77_0/stage/lib

win32 {
BDB_INCLUDE_PATH=$$BASE_DIR/db-6.2.32/build_windows
BDB_LIB_PATH=$$BASE_DIR/db-6.2.32/build_windows
}
!win32 {
BDB_INCLUDE_PATH=$$BASE_DIR/db-6.2.32/build_unix
BDB_LIB_PATH=$$BASE_DIR/db-6.2.32/build_unix
}

OPENSSL_INCLUDE_PATH=$$BASE_DIR/openssl-3.4.0/include
OPENSSL_LIB_PATH=$$BASE_DIR/openssl-3.4.0
SECP256K1_INCLUDE_PATH=$$BASE_DIR/secp256k1-0.7.1/include $$BASE_DIR/secp256k1-0.7.1/contrib
SECP256K1_LIB_PATH=$$BASE_DIR/secp256k1-0.7.1/.libs

QRENCODE_INCLUDE_PATH=$$BASE_DIR/libqrencode-4.1.1
QRENCODE_LIB_PATH=$$BASE_DIR/libqrencode-4.1.1

QRDECODE_INCLUDE_PATH=$$BASE_DIR/qzxing-3.3.0/src
QRDECODE_LIB_PATH=$$BASE_DIR/qzxing-3.3.0/src

CURL_INCLUDE_PATH=$$BASE_DIR/curl-8.11.0/include
CURL_LIB_PATH=$$BASE_DIR/curl-8.11.0/lib/.libs

# Custom zlib built WITHOUT stack protector
ZLIB_INCLUDE_PATH=$$BASE_DIR/zlib-1.3.2
ZLIB_LIB_PATH=$$BASE_DIR/zlib-1.3.2

win32 {
MINGW_INCLUDE_PATH=C:/msys64/mingw64/include
MINGW_LIB_PATH=C:/msys64/mingw64/lib
}

# use: qmake "RELEASE=1"
contains(RELEASE, 1) {
    # Mac: compile for maximum compatibility (10.5, 32-bit)
    macx:QMAKE_CXXFLAGS += -mmacosx-version-min=10.5 -arch x86_64 -isysroot /Developer/SDKs/MacOSX11.3.sdk

    !windows:!macx {
        # Linux: static link
        LIBS += -Wl,-Bstatic
    }
}

contains(DEBUG, 1) {
    message(Building with DEBUG)
    QMAKE_CXXFLAGS -= -O2
    QMAKE_CFLAGS -= -O2

    QMAKE_CFLAGS += -g -O0
    QMAKE_CXXCFLAGS += -g -O0
}

# Custom zlib (built without stack protector) MUST be searched before /mingw64/lib
# This ensures Qt's auto-added -lz resolves to our safe libz.a
win32:QMAKE_LFLAGS += -L$$ZLIB_LIB_PATH

# for debugging at release mode
#QMAKE_CXXFLAGS +=-g
#win32:QMAKE_LFLAGS_RELEASE -= -Wl,-s
#QMAKE_CXXFLAGS_RELEASE += $$QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO
#QMAKE_LFLAGS_RELEASE += $$QMAKE_LFLAGS_RELEASE_WITH_DEBUGINFO

# use: qmake "USE_QRCODE=1"
# libqrencode (http://fukuchi.org/works/qrencode/index.en.html) must be installed for support
contains(USE_QRCODE, 1) {
    message(Building with QRCode support)
    DEFINES += USE_QRCODE
    macx:LIBS += $$QRENCODE_LIB_PATH/libqrencode.a
    else:LIBS += -lqrencode
}

# use: qmake "USE_DBUS=1"
contains(USE_DBUS, 1) {
    message(Building with DBUS (Freedesktop notifications) support)
    DEFINES += USE_DBUS
    QT += dbus
}

contains(BITCOIN_NEED_QT_PLUGINS, 1) {
    DEFINES += BITCOIN_NEED_QT_PLUGINS
    QTPLUGIN += qcncodecs qjpcodecs qtwcodecs qkrcodecs qtaccessiblewidgets
}

contains(USE_RASPBERRY, 1) {
    message(Building with Raspberry support)
    DEFINES += RASPBERRY
    QTPLUGIN += qeglfs qlinuxfb qminimal qminimalegl qoffscreen qvnc qwebgl qxcb
}

contains(USE_UPNP, 1) {
    message(Building with miniupnpc support)
    INCLUDEPATHS += -I$$BASE_DIR/miniupnpc-2.2.3
    MINIUPNPC_LIB_PATH=$$BASE_DIR/miniupnpc-2.2.3
    LIBS += $$join(MINIUPNPC_LIB_PATH,,-L,) -lminiupnpc
    win32:LIBS += -liphlpapi
    DEFS += -DSTATICLIB -DUSE_UPNP=$(USE_UPNP)
}

# use: qmake "USE_SSL=1"
contains(USE_SSL, 1) {
    message(Building with SSL support for RPC)
    DEFINES += USE_SSL
}

# use: qmake "USE_IPV6=1" ( enabled by default; default)
#  or: qmake "USE_IPV6=0" (disabled by default)
#  or: qmake "USE_IPV6=-" (not supported)
contains(USE_IPV6, -) {
    message(Building without IPv6 support)
} else {
    count(USE_IPV6, 0) {
        USE_IPV6=1
    }
    DEFINES += USE_IPV6=$$USE_IPV6
}

INCLUDEPATH += src/leveldb/include src/leveldb/helpers
LIBS += $$PWD/src/leveldb/libleveldb.a $$PWD/src/leveldb/libmemenv.a
SOURCES += src/txdb-leveldb.cpp

!win32 {
    # we use QMAKE_CXXFLAGS_RELEASE even without RELEASE=1 because we use RELEASE to indicate linking preferences not -O preferences
    genleveldb.commands = cd $$PWD/src/leveldb && CC=$$QMAKE_CC CXX=$$QMAKE_CXX $(MAKE) OPT=\"$$QMAKE_CXXFLAGS $$QMAKE_CXXFLAGS_RELEASE\" libleveldb.a libmemenv.a
	} else {
    # make an educated guess about what the ranlib command is called
    isEmpty(QMAKE_RANLIB) {
        QMAKE_RANLIB = $$replace(QMAKE_STRIP, strip, ranlib)
    }
    LIBS += -lshlwapi
    genleveldb.commands = cd $$PWD/src/leveldb && CC=$$QMAKE_CC CXX=$$QMAKE_CXX TARGET_OS=OS_WINDOWS_CROSSCOMPILE $(MAKE) OPT=\"$$QMAKE_CXXFLAGS $$QMAKE_CXXFLAGS_RELEASE\" libleveldb.a libmemenv.a && $$QMAKE_RANLIB $$PWD/src/leveldb/libleveldb.a && $$QMAKE_RANLIB $$PWD/src/leveldb/libmemenv.a
}

genleveldb.target = $$PWD/src/leveldb/libleveldb.a
genleveldb.depends = FORCE

contains(USE_O3, 1) {
    message(Building O3 optimization flag)
    QMAKE_CXXFLAGS_RELEASE -= -O2
    QMAKE_CFLAGS_RELEASE -= -O2
    QMAKE_CXXFLAGS += -O3
    QMAKE_CFLAGS += -O3
}

*-g++-32 {
    message("32 platform, adding -msse2 flag")

    QMAKE_CXXFLAGS += -msse2
    QMAKE_CFLAGS += -msse2
}

QMAKE_CXXFLAGS_WARN_ON = -fdiagnostics-show-option -Wall -Wpedantic -Wextra -Wno-ignored-qualifiers -Wformat -Wformat-security -Wno-unused-parameter -Wstack-protector -fexceptions


# Input
DEPENDPATH += src src/json src/qt
HEADERS += src/qt/bitcoingui.h \
    src/curlnet.h \
    src/qt/intro.h \
    src/qt/jsonhighlighter.h \
    src/qt/openbrowserdialog.h \
    src/qt/transactiontablemodel.h \
    src/qt/addresstablemodel.h \
    src/qt/optionsdialog.h \
    src/qt/coincontroldialog.h \
    src/qt/coincontroltreewidget.h \
    src/qt/sendcoinsdialog.h \
    src/qt/addressbookpage.h \
    src/qt/signverifymessagedialog.h \
    src/qt/aboutdialog.h \
    src/qt/editaddressdialog.h \
    src/qt/bitcoinaddressvalidator.h \
    src/alert.h \
    src/addrman.h \
    src/base58.h \
    src/bignum.h \
    src/checkpoints.h \
    src/compat.h \
    src/coincontrol.h \
    src/sync.h \
    src/util.h \
    src/uint256.h \
    src/kernel.h \
    src/scrypt.h \
    src/pbkdf2.h \
    src/zerocoin/Accumulator.h \
    src/zerocoin/AccumulatorProofOfKnowledge.h \
    src/zerocoin/Coin.h \
    src/zerocoin/CoinSpend.h \
    src/zerocoin/Commitment.h \
    src/zerocoin/ParamGeneration.h \
    src/zerocoin/Params.h \
    src/zerocoin/SerialNumberSignatureOfKnowledge.h \
    src/zerocoin/SpendMetaData.h \
    src/zerocoin/ZeroTest.h \
    src/zerocoin/Zerocoin.h \
    src/serialize.h \
    src/strlcpy.h \
    src/main.h \
    src/miner.h \
    src/net.h \
    src/key.h \
    src/db.h \
    src/txdb.h \
    src/walletdb.h \
    src/script.h \
    src/init.h \
    src/irc.h \
    src/mruset.h \
    src/json/json_spirit_writer_template.h \
    src/json/json_spirit_writer.h \
    src/json/json_spirit_value.h \
    src/json/json_spirit_utils.h \
    src/json/json_spirit_stream_reader.h \
    src/json/json_spirit_reader_template.h \
    src/json/json_spirit_reader.h \
    src/json/json_spirit_error_position.h \
    src/json/json_spirit.h \
    src/qt/clientmodel.h \
    src/qt/guiutil.h \
    src/qt/transactionrecord.h \
    src/qt/guiconstants.h \
    src/qt/optionsmodel.h \
    src/qt/monitoreddatamapper.h \
    src/qt/transactiondesc.h \
    src/qt/transactiondescdialog.h \
    src/qt/bitcoinamountfield.h \
    src/wallet.h \
    src/keystore.h \
    src/qt/transactionfilterproxy.h \
    src/qt/transactionview.h \
    src/qt/walletmodel.h \
    src/bitcoinrpc.h \
    src/qt/overviewpage.h \
    src/qt/csvmodelwriter.h \
    src/crypter.h \
    src/qt/sendcoinsentry.h \
    src/qt/qvalidatedlineedit.h \
    src/qt/bitcoinunits.h \
    src/qt/qvaluecombobox.h \
    src/qt/askpassphrasedialog.h \
    src/protocol.h \
    src/qt/notificator.h \
    src/qt/qtipcserver.h \
    src/allocators.h \
    src/ui_interface.h \
    src/qt/rpcconsole.h \
    src/version.h \
    src/netbase.h \
    src/clientversion.h \
    src/threadsafety.h \
    src/checkqueue.h \
    src/timestamps.h \
    src/qt/qtcamera.h \
    src/qt/trafficgraphwidget.h \
    src/ntp.h \
    src/qt/chatworker.h \
    src/qt/chatwidget.h \
    src/memusage.h \
    src/qt/burncoinsentry.h \
    src/qt/burncoinsdialog.h \
    src/qt/blockbrowser.h \
    src/qt/sendtimelockdialog.h \
    src/qt/calctimestampdlg.h \
	src/secp256k1/contrib/lax_der_parsing.h \
	src/secp256k1/contrib/lax_der_privatekey_parsing.h \
	src/sqlite3/sqlite3.h \
	src/sqlitewallet.h \
	src/walletmigrate.h

SOURCES += src/qt/bitcoin.cpp src/qt/bitcoingui.cpp \
    src/qt/intro.cpp \
    src/qt/openbrowserdialog.cpp \
    src/qt/transactiontablemodel.cpp \
    src/qt/addresstablemodel.cpp \
    src/qt/optionsdialog.cpp \
    src/qt/sendcoinsdialog.cpp \
    src/qt/coincontroldialog.cpp \
    src/qt/coincontroltreewidget.cpp \
    src/qt/addressbookpage.cpp \
    src/qt/signverifymessagedialog.cpp \
    src/qt/aboutdialog.cpp \
    src/qt/editaddressdialog.cpp \
    src/qt/bitcoinaddressvalidator.cpp \
    src/alert.cpp \
    src/version.cpp \
    src/sync.cpp \
    src/util.cpp \
    src/netbase.cpp \
	src/key.cpp \
    src/script.cpp \
    src/main.cpp \
    src/miner.cpp \
    src/init.cpp \
    src/net.cpp \
    src/irc.cpp \
    src/checkpoints.cpp \
    src/addrman.cpp \
    src/db.cpp \
    src/walletdb.cpp \
    src/qt/clientmodel.cpp \
    src/qt/guiutil.cpp \
    src/qt/transactionrecord.cpp \
    src/qt/optionsmodel.cpp \
    src/qt/monitoreddatamapper.cpp \
    src/qt/transactiondesc.cpp \
    src/qt/transactiondescdialog.cpp \
    src/qt/bitcoinstrings.cpp \
    src/qt/bitcoinamountfield.cpp \
    src/wallet.cpp \
    src/keystore.cpp \
    src/qt/transactionfilterproxy.cpp \
    src/qt/transactionview.cpp \
    src/qt/walletmodel.cpp \
    src/bitcoinrpc.cpp \
    src/rpcdump.cpp \
    src/rpcnet.cpp \
    src/rpcmining.cpp \
    src/rpcwallet.cpp \
    src/rpcblockchain.cpp \
    src/rpcrawtransaction.cpp \
    src/qt/overviewpage.cpp \
    src/qt/csvmodelwriter.cpp \
    src/crypter.cpp \
    src/qt/sendcoinsentry.cpp \
    src/qt/qvalidatedlineedit.cpp \
    src/qt/bitcoinunits.cpp \
    src/qt/qvaluecombobox.cpp \
    src/qt/askpassphrasedialog.cpp \
    src/protocol.cpp \
    src/qt/notificator.cpp \
    src/qt/qtipcserver.cpp \
    src/qt/rpcconsole.cpp \
    src/qt/qtcamera.cpp \
    src/qt/trafficgraphwidget.cpp \
    src/ntp.cpp \
    src/qt/chatwidget.cpp \
    src/qt/chatworker.cpp \
    src/qt/burncoinsentry.cpp \
    src/qt/burncoinsdialog.cpp \
    src/qt/blockbrowser.cpp \
    src/qt/sendtimelockdialog.cpp \
    src/qt/calctimestampdlg.cpp \
    src/noui.cpp \
    src/kernel.cpp \
    src/scrypt-arm.S \
    src/scrypt-x86.S \
    src/scrypt-x86_64.S \
    src/scrypt.cpp \
    src/pbkdf2.cpp \
    src/zerocoin/Accumulator.cpp \
    src/zerocoin/AccumulatorProofOfKnowledge.cpp \
    src/zerocoin/Coin.cpp \
    src/zerocoin/CoinSpend.cpp \
    src/zerocoin/Commitment.cpp \
    src/zerocoin/ParamGeneration.cpp \
    src/zerocoin/Params.cpp \
    src/zerocoin/SerialNumberSignatureOfKnowledge.cpp \
    src/zerocoin/SpendMetaData.cpp \
    src/zerocoin/ZeroTest.cpp \
	src/secp256k1/contrib/lax_der_parsing.c \
	src/secp256k1/contrib/lax_der_privatekey_parsing.c \
	src/sqlite3/sqlite3.c \
	src/sqlitewallet.cpp \
    src/walletmigrate.cpp

RESOURCES += \
    src/qt/bitcoin.qrc \
    src/qt/res/qdarkstyle/dark/darkstyle.qrc

FORMS += \
    src/qt/forms/blockbrowser.ui \
    src/qt/forms/burncoinsdialog.ui \
    src/qt/forms/burncoinsentry.ui \
    src/qt/forms/calctimestampdlg.ui \
    src/qt/forms/chatwidget.ui \
    src/qt/forms/intro.ui \
    src/qt/forms/coincontroldialog.ui \
    src/qt/forms/openbrowserdialog.ui \
    src/qt/forms/sendcoinsdialog.ui \
    src/qt/forms/addressbookpage.ui \
    src/qt/forms/sendtimelockdialog.ui \
    src/qt/forms/signverifymessagedialog.ui \
    src/qt/forms/aboutdialog.ui \
    src/qt/forms/editaddressdialog.ui \
    src/qt/forms/transactiondescdialog.ui \
    src/qt/forms/overviewpage.ui \
    src/qt/forms/sendcoinsentry.ui \
    src/qt/forms/askpassphrasedialog.ui \
    src/qt/forms/rpcconsole.ui \
    src/qt/forms/optionsdialog.ui

contains(USE_QRCODE, 1) {
    HEADERS += src/qt/qrcodedialog.h
    SOURCES += src/qt/qrcodedialog.cpp
    FORMS += src/qt/forms/qrcodedialog.ui
}

CODECFORTR = UTF-8

# for lrelease/lupdate
# also add new translations to src/qt/bitcoin.qrc under translations/
TRANSLATIONS = $$files(src/qt/locale/bitcoin_*.ts)

isEmpty(QMAKE_LRELEASE) {
    win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    else:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
}

isEmpty(QM_DIR):QM_DIR = $$PWD/src/qt/locale

# automatically build translations, so they can be included in resource file
TSQM.name = lrelease ${QMAKE_FILE_IN}
TSQM.input = TRANSLATIONS
TSQM.output = $$QM_DIR/${QMAKE_FILE_BASE}.qm
TSQM.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
TSQM.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += TSQM
PRE_TARGETDEPS += compiler_TSQM_make_all

# "Other files" is to show in Qt Creator
OTHER_FILES += \
    doc/*.rst doc/*.txt doc/README README.md res/bitcoin-qt.rc


windows:DEFINES += WIN32
windows:RC_FILE = src/qt/res/bitcoin-qt.rc

# Windows system libraries MUST come BEFORE OpenSSL
windows:LIBS += -lcrypt32 -lws2_32 -ladvapi32 -lgdi32 -luser32

# Set libraries and includes at end, to use platform-defined defaults if not overridden
INCLUDEPATH += $$BOOST_INCLUDE_PATH $$BDB_INCLUDE_PATH $$OPENSSL_INCLUDE_PATH $$SECP256K1_INCLUDE_PATH $$QRENCODE_INCLUDE_PATH $$QRDECODE_INCLUDE_PATH $$ZLIB_INCLUDE_PATH
# Windows: IMPORTANT: ZLIB_INCLUDE_PATH must come BEFORE MINGW_INCLUDE_PATH so our zlib.h is found first
windows:INCLUDEPATH += $$MINGW_INCLUDE_PATH
DEPENDPATH += $$BOOST_LIB_PATH

LIBS += $$join(BOOST_LIB_PATH,,-L,) $$join(BDB_LIB_PATH,,-L,) $$join(OPENSSL_LIB_PATH,,-L,) $$join(SECP256K1_LIB_PATH,,-L,) $$join(QRENCODE_LIB_PATH,,-L,) $$join(QRDECODE_LIB_PATH,,-L,)
LIBS +=  -lssl -lcrypto -lsecp256k1 -ldb_cxx

windows {
QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++
QMAKE_LFLAGS_RELEASE += -static -static-libgcc -static-libstdc++
QMAKE_CXXFLAGS += -static-libgcc -static-libstdc++

LIBS += -lws2_32 -lshlwapi -lmswsock -lole32 -loleaut32 -luuid -lgdi32 -lcrypt32
}

LIBS += -lboost_system$$BOOST_LIB_SUFFIX -lboost_filesystem$$BOOST_LIB_SUFFIX  -lboost_program_options$$BOOST_LIB_SUFFIX -lboost_thread$$BOOST_LIB_SUFFIX -lboost_chrono$$BOOST_LIB_SUFFIX

LIBS += -lqzxing

LIBS += $$join(ZLIB_LIB_PATH,,-L,)
DEPENDPATH += $$ZLIB_LIB_PATH
# Windows: IMPORTANT: custom zlib path MUST come BEFORE MINGW_LIB_PATH so our libz.a is found first
windows {
LIBS += $$join(MINGW_LIB_PATH,,-L,)
DEPENDPATH += $$MINGW_LIB_PATH
}

contains(RELEASE, 1) {
    !windows:!macx {
        # Linux: turn dynamic linking back on for c/c++ runtime libraries
        LIBS += -Wl,-Bdynamic
    }
}

!windows:!macx {
    DEFINES += LINUX
    LIBS += -lrt -ldl
}

win32:LIBS += -Wl,-Bstatic \
              -liconv \
              -lzstd \
              -lstdc++ \
              -lpthread \
              -lwinpthread \
              -lgcc_eh \
              -lgcc
			  
system($$QMAKE_LRELEASE -silent $$_PRO_FILE_)
