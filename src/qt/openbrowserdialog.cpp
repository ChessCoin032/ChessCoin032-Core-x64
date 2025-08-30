#include "openbrowserdialog.h"
#include "ui_openbrowserdialog.h"
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QDir>
#include <QSettings>
#include <QProcess>
#include <QMessageBox>

QOpenBrowserDialog::QOpenBrowserDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::openbrowserdialog),
    browserPath("")
{
    ui->setupUi(this);

    setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowCloseButtonHint);

#if (defined (WIN32) || defined (WIN64))
    setMinimumSize(500, 310);
    setMaximumSize(500, 310);
    resize(500, 310);
    ui->labelSelPath->setText(QString("Find and select the ChessCoinBrowser.exe file to continue"));
#elif (defined (LINUX) || defined (__linux__))
    setMinimumSize(580, 310);
    setMaximumSize(580, 310);
    resize(580, 310);
    ui->labelSelPath->setText(QString("Find and select the ChessCoinBrowser file to continue"));
#else
    setMinimumSize(600, 350);
    setMaximumSize(600, 350);
    resize(600, 350);
    ui->labelSelPath->setText(QString("Find and select the ChessCoinBrowser file to continue"));
#endif

    ui->downloadBtn->setEnabled(false);
    ui->downloadBtn->hide();

    QSettings settings;
    browserPath = settings.value("browserPath", "").toString();

    updateUIWithBrowserInfo();
}

QOpenBrowserDialog::~QOpenBrowserDialog()
{
    delete ui;
}


void QOpenBrowserDialog::on_downloadBtn_clicked()
{
    QString url = "https://update.chesscoin032.com:1032/downloads/browser/index.html";
    QDesktopServices::openUrl(QUrl(url));
}


void QOpenBrowserDialog::on_folderBtn_clicked()
{
    if (!browserPath.isEmpty())
    {
        QString browserDir = QFileInfo(browserPath).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(browserDir));
    }
}


void QOpenBrowserDialog::on_closeBtn_clicked()
{
    accept();
}

QString QOpenBrowserDialog::humanReadableSize(qint64 bytes)
{
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = 1024 * KB;
    constexpr qint64 GB = 1024 * MB;

    if (bytes >= GB)
        return QString::number(bytes / (double)GB, 'f', 2) + " GB";
    else if (bytes >= MB)
        return QString::number(bytes / (double)MB, 'f', 2) + " MB";
    else if (bytes >= KB)
        return QString::number(bytes / (double)KB, 'f', 2) + " KB";
    else
        return QString::number(bytes) + " bytes";
}

QString QOpenBrowserDialog::elidePath(const QString& path, int maxChars)
{
    if (path.length() <= maxChars)
        return path;

    QString start = path.left(maxChars / 2 - 2);
    QString end = path.right(maxChars / 2 - 2);
    return start + "..." + end;
}

void QOpenBrowserDialog::on_openBrowserBtn_clicked()
{
#ifdef Q_OS_WINDOWS
    QString appPath = QFileDialog::getOpenFileName(
        this,
        tr("Open ChessCoin browser app"),
        QDir::homePath(),
        tr("Executable Files (*.exe);;All Files (*)")
    );
#else
    QString appPath = QFileDialog::getOpenFileName(
        this,
        tr("Open ChessCoin browser app"),
        QDir::homePath(),
        tr("All Files (*)")
    );
#endif

    if (!appPath.isEmpty()) {

        browserPath = appPath;

        QSettings settings;
        settings.setValue("browserPath", browserPath);

        updateUIWithBrowserInfo();
    }
}

void QOpenBrowserDialog::updateUIWithBrowserInfo()
{
    if (!browserPath.isEmpty())
    {
        QString elided = elidePath(browserPath);
        ui->browserPathEdit->setText(elided);
        ui->browserPathEdit->setToolTip(browserPath);

        QFileInfo fileInfo(browserPath);

        ui->labelName->setText(fileInfo.fileName());

        qint64 fileSize = fileInfo.size();
        QString sizeString = humanReadableSize(fileSize);
        ui->labelSize->setText(sizeString);

        QString parentFolder = fileInfo.absolutePath();
        ui->labelDir->setText(parentFolder);

        ui->runBtn->show();
    }
    else
    {
        ui->runBtn->hide();
    }
}


void QOpenBrowserDialog::on_runBtn_clicked()
{
    if (browserPath.isEmpty() || !QFile::exists(browserPath)) {
        QMessageBox::warning(this, tr("Error"), tr("Browser executable not found!"));
        return;
    }

    // Start the process DETACHED (no parent-child relationship)
    bool success = QProcess::startDetached(browserPath, QStringList());

    if (!success) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to launch browser!"));
    }

    accept();
}


void QOpenBrowserDialog::on_clearBtn_clicked()
{
    browserPath = "";
    ui->browserPathEdit->clear();

    QSettings settings;
    settings.remove("browserPath");

    ui->labelName->setText("N/A");
    ui->labelSize->setText("N/A");
    ui->labelDir->setText("N/A");

    ui->runBtn->hide();
}

