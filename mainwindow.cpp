#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QtConcurrent>

#include <QString>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->label_3->setText("Введите путь к папке");

    ui->label_3->setText("Ready");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_dsc_btn_clicked()
{
    QString model_qt = ui->describe_le->text();
    std::string model = model_qt.toStdString();

    const std::string prompt =
        "Write a 2-3 sentence description of the image.";

    int port = GetPort();

    std::string cache_file = current_directory_ + "/descriptions.json";

    ui->label_3->setText("Processing...");

    QtConcurrent::run([=]() mutable {

        LoadDescriptionsFromFile(images_, cache_file);

        int sent = 0;

        for (auto& img : images_) {

            if (!img.GetDescription().empty()) {
                std::cout << "DESC SIZE: " << img.GetDescription().size() << std::endl;
                continue;
            }

            std::cout << "Sending to Ollama: "
                      << img.GetPath() << std::endl;

            std::string response =
                AskLLaVAVision(model, prompt, img.GetBase64(), port);

            img.SetDescription(response);
            sent++;
        }

        SaveDescriptionsToFile(images_, cache_file);

        std::cout << "Requests sent: " << sent << std::endl;

        QMetaObject::invokeMethod(this, [=]() {
            ui->label_3->setText(
                QString("Done. New requests: %1").arg(sent)
                );
        });
    });
}

void MainWindow::on_similar_btn_clicked()
{
    QString model_qt = ui->similar_le->text();
    std::string model = model_qt.toStdString();

    QString query_qt = ui->lineEdit->text();
    std::string query = query_qt.toStdString();

    int port = GetPort();

    std::vector<std::pair<std::string, double>> scored;

    for (auto& img : images_) {

        // ❗ пропускаем пустые описания
        if (img.GetDescription().empty())
            continue;

        std::string prompt =
            "You are a similarity scoring system.\n\n"
            "User query:\n\"" + query + "\"\n\n"
                      "Image description:\n\"" + img.GetDescription() + "\"\n\n"
                                     "Return ONLY a number from 0 to 100.";

        std::string response = AskLLaVAText(model, prompt, port);

        double score = 0.0;
        try {
            score = std::stod(response);
        } catch (...) {
            score = 0.0;
        }

        scored.emplace_back(img.GetPath(), score);
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    std::vector<QLabel*> labels = {
        ui->img1,
        ui->img2,
        ui->img3,
        ui->img4,
        ui->img5
    };

    for (int i = 0; i < labels.size(); ++i) {

        if (i < scored.size()) {
            QPixmap pixmap(
                QString::fromStdString(scored[i].first)
                );

            labels[i]->setPixmap(
                pixmap.scaled(150, 150, Qt::KeepAspectRatio)
                );
        } else {
            labels[i]->clear();
        }
    }

    QString output;

    for (int i = 0; i < std::min(5, (int)scored.size()); ++i) {
        output += QString::fromStdString(scored[i].first)
        + " (" + QString::number(scored[i].second) + "%)\n";
    }

    if (scored.empty()) {
        output = "No valid descriptions found";
    }

    ui->label_3->setText(output);
}


void MainWindow::on_directory_le_textChanged(const QString &text)
{
    std::string path = text.toStdString();
    std::replace(path.begin(), path.end(), '\\', '/');

    current_directory_ = path;

    images_ = LoadImagesFromDirectory(current_directory_);
    CreateBase64ForVector(images_);

    std::string cache_file = current_directory_ + "/descriptions.json";
    LoadDescriptionsFromFile(images_, cache_file);

    ui->label_3->setText("Directory loaded");
}



int MainWindow::GetPort() const
{
    if (ui->kport_cb->isChecked()) {
        bool ok = false;
        int port = ui->port_le->text().toInt(&ok);

        if (ok && port > 0 && port < 65536)
            return port;
    }

    return 11434; // дефолт Ollama
}

