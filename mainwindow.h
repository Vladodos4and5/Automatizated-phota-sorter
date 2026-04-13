#pragma once

#include <QMainWindow>
#include <vector>
#include "functions.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_dsc_btn_clicked();
    void on_similar_btn_clicked();
    int GetPort() const;
    void on_directory_le_textChanged(const QString &text);

private:
    Ui::MainWindow *ui;
    std::vector<Image> images_;
    std::string current_directory_;
};
