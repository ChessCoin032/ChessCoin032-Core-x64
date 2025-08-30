#ifndef OPENBROWSERDIALOG_H
#define OPENBROWSERDIALOG_H

#include <QDialog>

namespace Ui {
class openbrowserdialog;
}

class QOpenBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QOpenBrowserDialog(QWidget *parent = nullptr);
    ~QOpenBrowserDialog();

    QString getBroserAppPath() { return browserPath; }

private slots:
    void on_downloadBtn_clicked();
    void on_folderBtn_clicked();
    void on_closeBtn_clicked();
    void on_openBrowserBtn_clicked();
    void on_runBtn_clicked();
    void on_clearBtn_clicked();

private:
    QString humanReadableSize(qint64 bytes);
    QString elidePath(const QString& path, int maxChars = 70);

    void updateUIWithBrowserInfo();

private:
    Ui::openbrowserdialog *ui;

    QString browserPath;
};

#endif // OPENBROWSERDIALOG_H
