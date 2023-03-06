#include "mainwindow.h"
#include "./ui_mainwindow.h"


#include <QTranslator>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QMouseEvent>
#include <QPainter>
#include <QMimeData>
#include <filesystem>
#include <map>
#include <Eigen/Core>
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#define QT_NO_CAST_FROM_ASCII

extern QTranslator trans;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , backup_data_label(backup_num_maximun)
    , ui(new Ui::MainWindow)

{

    ui->setupUi(this);

    // 加载所有网络模型
    auto dir = QDir(":/models");
    auto list = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for(const auto &item:list){
        auto name = item.split(".")[0];
        QFile f(dir.absoluteFilePath(item));
        f.open(QFile::ReadOnly);
        auto array = f.read(f.size());
        if(nets.count(name) != 0){
            continue;
        }
        nets[name] = cv::dnn::readNetFromONNX(array.data(), array.size());
        QAction *action = new QAction(name, ui->menuModels);
        action->setCheckable(true);
        ui->menuModels->addAction(action);
    }
    if(!ui->menuModels->actions().empty()){
        ui->menuModels->actions()[0]->setChecked(true);
    }

    connect(ui->menuModels, &QMenu::triggered, this, &MainWindow::onChangeModel);

    connect(ui->menuLanguage, &QMenu::triggered, this, &MainWindow::onChangeLanguage);



    connect(ui->actionCreate, &QAction::triggered, this, &MainWindow::onCreate);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpen);

    connect(ui->listWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::onItemSelectionChanged);

    connect(ui->btnPrevious, &QPushButton::clicked, this, &MainWindow::onBtnPrevious);
    connect(ui->btnNext, &QPushButton::clicked, this, &MainWindow::onBtnNext);

    connect(ui->btnWipe, &QPushButton::clicked, this, &MainWindow::onBtnWipe);
    connect(ui->btnAutoLabel, &QPushButton::clicked, this, &MainWindow::onBtnAutoLabel);
    connect(ui->btnReset, &QPushButton::clicked, this, &MainWindow::onBtnReset);
    connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::onBtnSave);
    connect(ui->btnRemoveImage, &QPushButton::clicked, this, &MainWindow::onBtnRemoveImage);
    connect(ui->btnExport, &QPushButton::clicked, this, &MainWindow::onBtnExport);

    for(auto sld:{ui->sldPencilSize, ui->sldImageTransparency, ui->sldLabelTransparency}){

        auto func = [this](){
            auto size = double(ui->sldPencilSize->value())/ ui->sldPencilSize->maximum()
                        * max_size_pencil + 1;
            size_pencil = round(size);
            transparency_image = double(ui->sldImageTransparency->value()) / ui->sldImageTransparency->maximum();
            transparency_label = double(ui->sldLabelTransparency->value()) / ui->sldLabelTransparency->maximum();
//            qDebug() << "set " << size_pencil;
            ui->lblPencilSize->setText(QString("%1px").arg(size_pencil));
//            qDebug() << "now text: " << ui->lblPencilSize->text();
            ui->lblImageTransparency->setText(QString("%1%").arg(int(transparency_image*100)));
            ui->lblLabelTransparency->setText(QString("%1%").arg(int(transparency_label*100)));
            updateImageView(true);

        };
        connect(sld, &QSlider::valueChanged, func);
        sld->setRange(0,100);
        timer.callOnTimeout([sld](){sld->setValue(50);});
    }
    timer.setSingleShot(true);
    timer.start(50);

    for(auto lbl:{ui->lblImage, ui->lblLabel, ui->lblImageLabel}){
        lbl->setMouseTracking(true);
        lbl->installEventFilter(this);
    }

    ui->centralwidget->installEventFilter(this);
    for(auto x:ui->centralwidget->findChildren<QSlider*>()){
        x->installEventFilter(this);
    }
    ui->listWidget->installEventFilter(this);

    this->setAcceptDrops(true);
//    for(auto c:ui->centralwidget->children()){
//        c->installEventFilter(this);
//    }
    //启动任务线程
//    thread_task = std::thread([this](){
//        while(is_window_alive){
//            std::unique_lock lock(mtx_q_tasks);
//            cond_task.wait(lock,[this]{
//                return !q_tasks.empty();
//            });
//            auto func = q_tasks.front();
//            q_tasks.pop();
//            lock.unlock();
//            func();
//        }
//    });

//    connect(&timerUpdate, &QTimer::timeout, this, &MainWindow::updateImageView);
//    timerUpdate.start(1000/FPS);
}

void MainWindow::changeLanguage(const QString &name){
    static std::map<QString,QString> languages{
        {"English", "trans_en.qm"},
        {"中文","trans_ch.qm"}
    };
    if(languages.count(name)!=0){
        auto path = ":/translation/" + languages[name];
        if(trans.load(path)){
            ui->retranslateUi(this);
        }
    }
}
void MainWindow::onChangeLanguage(QAction *action){
    if(action == nullptr){
        return;
    }
    changeLanguage(action->text());
}

void MainWindow::onChangeModel(QAction *action){
    for(auto a:ui->menuModels->actions()){
        a->setChecked(false);
    }
    action->setChecked(true);
}

void MainWindow::onOpen(){
    QString pathPro;
    while(true){
        pathPro = QFileDialog::getOpenFileName(this, "", "", "*.pro");
        if(pathPro==""){
            return;
        }
        if(pathPro.endsWith(".pro")){
            break;
        }
        QMessageBox::warning(this, tr("无效的文件"), tr("请重新选择项目"));
    }
    openProject(pathPro);
}

void MainWindow::onCreate(){
    QString pathDir;
    while(true){
        pathDir = QFileDialog::getExistingDirectory(this);
        if(pathDir == ""){
            return;
        }
        QDir dir(pathDir);
        dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        if(dir.entryInfoList().empty()){
            break;
        }
        QMessageBox::warning(this, tr("文件夹非空"), tr("请选择或创建一个空的文件夹"));
    }
    QString pathPro = pathDir + "/" + nameSettings;
    QFile file(pathPro);
    file.open(QIODevice::WriteOnly);
    file.close();
    openProject(pathPro);
}


void MainWindow::openProject(const QString& pathPro){
    if(!QFile::exists(pathPro)){
        return;
    }
    QFileInfo info(pathPro);
    this->pathDir = info.path();
    QDir dir(pathDir);
    auto pathDirExport = dir.absoluteFilePath(this->nameDirExport);
    auto pathDirTrash = dir.absoluteFilePath(this->nameDirTrash);
    auto pathDirImage = dir.absoluteFilePath(this->nameDirImage);
    auto pathDirLabel = dir.absoluteFilePath(this->nameDirLabel);

    for(auto& path:{pathDirExport, pathDirTrash, pathDirImage, pathDirLabel}){
        if(!QFile::exists(path)){
            dir.mkdir(path);
        }
    }
    updateListWidget();
}

void MainWindow::resetData(){

//    std::scoped_lock lock(mtx_data_label, mtx_q_tasks);
//    while(!q_tasks.empty()){
//        q_tasks.pop();
//    }
    data_image = QImage();
    data_label = QImage();
    data_saved_label = QImage();
    idx_current = -1;
    backup_data_label.clear();
}
void MainWindow::updateListWidget(){
    QDir dir(this->pathDir);
    dir = QDir(dir.absoluteFilePath(this->nameDirImage));
    auto list = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    std::sort(list.begin(), list.end(), []
        (const QString&s1, const QString&s2){
        //先按数字从小到大排，然后按字母顺序排
        static QRegularExpression reg("^\\d+");
        auto match1 = reg.match(s1);
        auto match2 = reg.match(s2);
        if (match1.hasMatch() && !match2.hasMatch()){
            return true;
        }
        else if(!match1.hasMatch() && match2.hasMatch()){
            return false;
        }
        else if(!match1.hasMatch() && !match2.hasMatch()){
            return s1 < s2;
        }
        else{
            auto n1 = match1.captured(0).toInt();
            auto n2 = match2.captured(0).toInt();
            return n1<n2;
        }

    });

    auto idx_origin = ui->listWidget->currentRow();
    ui->listWidget->setCurrentRow(-1);
    ui->listWidget->clear();
    ui->listWidget->addItems(list);
    idx_origin = std::min(idx_origin, ui->listWidget->count()-1);
    ui->listWidget->setCurrentRow(idx_origin);

}


void MainWindow::updateImageView(bool must){
    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(now - last);
    if(!must){
        if(interval.count() < 1000/FPS_Update){
            return;
        }
    }

    last = now;
//    std::scoped_lock lock(mtx_data_label);
    std::vector<int> v;
    std::vector v2({1,2,3});
    for(auto lbl:{ui->lblImage, ui->lblLabel, ui->lblImageLabel}){
//        lbl->setAlignment(Qt::AlignCenter);
        lbl->setScaledContents(!is_fix_size);
    }
    ui->lblImage->setPixmap(QPixmap::fromImage(data_image));
    ui->lblLabel->setPixmap(QPixmap::fromImage(data_label));

    if(data_image.isNull() || data_label.isNull()){
        ui->lblImageLabel->setPixmap(QPixmap());
        return;
    }
//    qDebug() << "mat create begin!";

    cv::Mat mat_image(data_image.height(), data_image.width(), CV_8UC4);
    std::memcpy(mat_image.data, data_image.bits(), data_image.sizeInBytes());
    mat_image = mat_image * (1-transparency_image);

    cv::Mat mat_label(data_label.height(), data_label.width(),  CV_8UC4);
    std::memcpy(mat_label.data, data_label.bits(), data_label.sizeInBytes());
    cv::cvtColor(mat_label * (1-transparency_label), mat_label, cv::COLOR_BGRA2GRAY);
//    qDebug() << "mat created!";


    auto mat_mixed = mat_image.clone();
//    mat_mixed.convertTo(mat_mixed,CV_16SC4);
    std::vector<cv::Mat> channels;
    cv::split(mat_mixed, channels);
    channels[0] -= mat_label;
    channels[1] -= mat_label;
    channels[2] += mat_label;
    cv::merge(channels, mat_mixed);
//    cv::imshow("test", mat_mixed);

//    mat_mixed.convertTo(mat_mixed,CV_8UC4);
    QImage image_mixed(data_image.width(), data_image.height(),QImage::Format_RGB32);
    std::memcpy(image_mixed.bits(), mat_mixed.data, image_mixed.sizeInBytes());
    ui->lblImageLabel->setPixmap(QPixmap::fromImage(image_mixed));




}

QImage drawRectOrCircle(double size, QColor color, QColor background_color);
void MainWindow::updateCursor(){
    if(lblHovering==nullptr || lblHovering->pixmap().isNull()){
        setCursor(Qt::ArrowCursor);
        return;
    }
    else{
        QPixmap pixmap(1,1);
        pixmap.fill(QColor(0,0,0,0));
        setCursor(QCursor(pixmap));
    }
}

//void MainWindow::saveLabel(){

//}

void MainWindow::backupLabel(){
    if(data_label.isNull()){
        return;
    }
    if(backup_data_label.size() >= (backup_num_maximun-1)){
        backup_data_label.pop_front();
    }
    backup_data_label.push_back(data_label.copy());
}

void MainWindow::restoreBackupLabel(){
    if(backup_data_label.empty()){
        return;
    }
    data_label = std::move(backup_data_label.back());
    backup_data_label.pop_back();
    updateImageView(true);
}

void MainWindow::onBtnPrevious(){
    int idx = std::max(ui->listWidget->currentRow()-1, 0);
    ui->listWidget->setCurrentRow(idx);
}

void MainWindow::onBtnNext(){
    int count = ui->listWidget->count();
    int idx = std::min(count-1, ui->listWidget->currentRow()+1);
    ui->listWidget->setCurrentRow(idx);
}

void MainWindow::onBtnWipe(){
    if(data_label.isNull()){
        return;
    }
    backupLabel();
    data_label = QImage(data_image.size(),data_image.format());
    data_label.fill(QColor(Qt::black));
    updateImageView(true);
}

void MainWindow::onBtnAutoLabel(){
    if(data_image.isNull()){
        return;
    }
    const QAction *selectedModel = nullptr;
    for(const auto *a:ui->menuModels->actions()){
        if(a->isChecked()){
            selectedModel = a;
            break;
        }
    }
    if(selectedModel == nullptr){
        return;
    }
    backupLabel();
    cv::Mat mat_image(data_image.height(), data_image.width(),  CV_8UC4);
    qDebug() << mat_image.dims;
    std::memcpy(mat_image.data, data_image.bits(), data_image.sizeInBytes());
    cv::cvtColor(mat_image, mat_image, cv::COLOR_BGRA2GRAY);
    auto &net = nets[selectedModel->text()];

    auto blob = cv::dnn::dnn4_v20220524::blobFromImage(mat_image, 1.0/255);



    net.setInput(blob);
    auto pred = net.forward();

    cv::Mat mat_label = pred.reshape(1,mat_image.rows) * 255;
    mat_label.convertTo(mat_label, CV_8UC1);


    cv::threshold(mat_label, mat_label, 10, 255,cv::THRESH_BINARY);

    cv::cvtColor(mat_label, mat_label, cv::COLOR_GRAY2BGRA);


    memcpy(data_label.bits(), mat_label.data, data_label.sizeInBytes());
    updateImageView(true);
}

void MainWindow::onBtnReset(){
    data_label = data_saved_label.copy();
    updateImageView(true);
}

void MainWindow::onBtnSave(){
    if(data_label.isNull()){
        return;
    }
    auto name = ui->listWidget->item(idx_current)->text();
    auto path = pathDir + "/" + nameDirLabel + "/" + name;
    auto temp = data_label.convertedTo(QImage::Format_Grayscale8);
    temp.save(path, nullptr, 100);
    data_saved_label = data_label;
}

void moveFile(const QString &path_old,  const QString &path_new, bool copy = false){
    QFile f_old(path_old);
    QFile f_new(path_new);
    if(!f_old.exists()||!f_old.open(QFile::ReadOnly) || !f_new.open(QFile::WriteOnly)){
        qDebug() << "File error!";
        return;
    }
    f_new.write(f_old.read(f_old.size()));
    if(!copy){
        f_old.remove();
    }

}

void MainWindow::onBtnRemoveImage(){
    if(idx_current == -1){
        return;
    }
    auto name = ui->listWidget->item(idx_current)->text();
    auto path_old_image = pathDir + "/" + nameDirImage + "/" + name;
    auto path_new_image = pathDir + "/" + nameDirTrash + "/" + name;
    moveFile(path_old_image, path_new_image);

    auto path_label = pathDir + "/" + nameDirLabel + name;
    QFile f(path_label);
    if(f.exists()){
        f.remove();
    }
    QMessageBox::information(this, tr("完成"), tr("已将%1移动到%2").arg(name).arg(path_new_image));
    updateListWidget();
}

void MainWindow::onBtnExport(){
    std::vector<QString> ls_name_exportable;
    for(int i=0;i<ui->listWidget->count();i++){
        auto item = ui->listWidget->item(i);
        auto name = item->text();
        auto path_image = pathDir + "/" + nameDirImage + "/" + name;
        auto path_label = pathDir + "/" + nameDirLabel + "/" + name;
        if(QFile::exists(path_image) && QFile::exists(path_label)){
            ls_name_exportable.push_back(name);
        }
    }
    if(ls_name_exportable.empty()){
        return;
    }
    QMessageBox::information(this, tr("提示"), tr("会将%1张已标注的图片及标注转移到目标文件夹").arg(ls_name_exportable.size()));
    auto dir_export = QFileDialog::getExistingDirectory(this, tr("选择导出到的文件夹"), pathDir + "/" + nameDirExport);
    if(dir_export==""){
        return;
    }
    auto dir_export_image = dir_export + "/image";
    auto dir_export_label = dir_export + "/label";
    if(!QFile::exists(dir_export_image)){
        QDir().mkdir(dir_export_image);
    }
    if(!QFile::exists(dir_export_label)){
        QDir().mkdir(dir_export_label);
    }
    for(const auto &name:ls_name_exportable){
        auto path_image_old = pathDir + "/" + nameDirImage + "/" + name;
        auto path_image_new = dir_export_image + "/" + name;
        moveFile(path_image_old, path_image_new);
        auto path_label_old = pathDir + "/" + nameDirLabel + "/" + name;
        auto path_label_new = dir_export_label + "/" + name;
        moveFile(path_label_old, path_label_new);
    }
    QMessageBox::information(this, tr("完成"),tr("已将%1张图片及标注导出到'%2'").arg(ls_name_exportable.size()).arg(dir_export));
    updateListWidget();
    ui->listWidget->setCurrentRow(0);
}

void MainWindow::onItemSelectionChanged(){
//    std::scoped_lock lock(mtx_q_tasks);
//    std::unique_lock lock_data(mtx_data_label);
    if(idx_current == ui->listWidget->currentRow()){
        return;
    }

//    while(!q_tasks.empty()){
//        q_tasks.pop();
//    }

    if(ui->listWidget->currentRow() == -1){
        resetData();
        updateImageView(true);
        return;
    }

    bool is_should_save = false;
    if(!data_label.isNull()){
        if(data_label != data_saved_label){
            is_should_save = true;
        }
    }
    if(is_should_save){
        QMessageBox msgBox(this);
        msgBox.setModal(true);
        msgBox.setWindowTitle(tr("未保存"));
        msgBox.setText(tr("是否保存更改？"));
        auto btnSave = msgBox.addButton(tr("保存"),QMessageBox::ButtonRole::YesRole);
        auto btnDiscard = msgBox.addButton(tr("放弃"),QMessageBox::ButtonRole::NoRole);
        msgBox.exec();
        if(msgBox.clickedButton() == btnSave){
            onBtnSave();
        }
        else if(msgBox.clickedButton() == btnDiscard){

        }
        else{
            ui->listWidget->setCurrentRow(idx_current);
        }
    }


    idx_current = ui->listWidget->currentRow();
    auto item = ui->listWidget->currentItem();


    auto name_item = item->text();
    auto path = pathDir + "/" + nameDirImage + "/" + name_item;

    if(!data_image.load(path)){
        QMessageBox::critical(this, tr("错误"), tr("文件不存在！"));
        updateListWidget();
        return;
    }
    data_image = data_image.convertToFormat(QImage::Format_RGB32);
    path = pathDir + "/" + nameDirLabel + "/" + name_item;

    if(QFile::exists(path)){

        data_saved_label.load(path);
        data_saved_label.convertTo(data_image.format());

    }
    else{
        data_saved_label = QImage(data_image.size(),data_image.format());
        data_saved_label.fill(QColor(Qt::black));
    }
    data_label = data_saved_label.copy();


//    lock_data.unlock();
    updateImageView(true);
}



void drawRectOrCircle(QPaintDevice& image, QPointF center, double size, QColor color){
    QPainter painter(&image);
    QBrush brush(color);
    painter.setBrush(brush);

    if(size <= 3){
        QPointF lefttop = QPointF(center.x()-size/2,center.y()-size/2);
        QRectF rect(lefttop, QSizeF(size/2, size/2));
        painter.setPen(color);
        painter.drawRect(rect);
    }else{
        painter.setPen(QColor(0,0,0,0));
        painter.drawEllipse(center,size/2, size/2);
    }
}

QImage drawRectOrCircle(double size, QColor color, QColor background_color){
    QImage image(QSizeF(size+5,size+5).toSize(), QImage::Format_RGBA8888);
    image.fill(background_color);
    drawRectOrCircle(image, QPointF(image.width()/2.0, image.height()/2.0), size, color);
    return image;
}
//void draw
bool MainWindow::eventFilter(QObject *obj, QEvent *e){
    static const std::vector vec_lbl{ui->lblImage, ui->lblLabel, ui->lblImageLabel};
    if(std::find(vec_lbl.cbegin(), vec_lbl.cend(), obj) != vec_lbl.cend()){
        QLabel *label = (QLabel *)obj;
        static QPointF lastDrawPos = QPoint();
        static QLabel *lastDrawLabel = nullptr;
        if(e->type() == QMouseEvent::Enter){
            lblHovering = label;
            updateCursor();
        }
        else if(e->type() == QMouseEvent::Leave){
            lblHovering = nullptr;
            lastDrawLabel = nullptr;
            updateCursor();
        }
        if(e->type() == QEvent::Paint){
            if(label->pixmap().isNull()){
                return true;
            }
            //绘制图片
            const int width_border = 1;
            QSize size = QSize(label->width() - 2*width_border, label->height() - 2*width_border);
            if(size.width() <= 0 || size.height() <= 0){
                return true;
            }
            QPixmap pixmap = label->pixmap().scaled(label->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            //绘制鼠标
            if(lblHovering != nullptr){
                double h_factor = double(size.height()) / data_image.height();
                double w_factor = double(size.width()) / data_image.width();
                double scale_factor = std::min(h_factor, w_factor);
                auto size_cursor = 10*scale_factor;
                QColor color = QColor(0,255,255,128);
                if(lblHovering == label){
                    size_cursor = size_pencil*scale_factor;
                    size_cursor = std::max(6.0, size_cursor);
                    color = QColor(255,255,0,255);
                }
                scale_factor = std::max(1.0,scale_factor);
                auto pos_cursor = lblHovering->mapFromGlobal(QCursor().pos());
                drawRectOrCircle(pixmap, pos_cursor, size_cursor, color);
            }

            QPainter painter(label);
            painter.drawPixmap(QPointF(width_border, width_border),pixmap);

            return true;
        }
        QMouseEvent *me = dynamic_cast<QMouseEvent *>(e);
        if(me != nullptr){
//            label->repaint();
            //以下属于绘制事件响应
            if(me->type() == QMouseEvent::MouseButtonPress){
                backupLabel();
            }
            int drawValue = -1;
            if(me->buttons()&Qt::LeftButton){
                drawValue =  255;
            }
            else if(me->buttons()& Qt::RightButton){
                drawValue =  0;
            }
            if(drawValue == -1){
                lastDrawLabel = nullptr;
                // 重绘label，更新鼠标位置
                for(auto l:{ui->lblImageLabel, ui->lblImage, ui->lblLabel}){
                    l->repaint();
                }
                return true;
            }
            auto pos = me->position();
            int size = size_pencil;
            auto funcDraw = [drawValue, this, pos, label, size]{

                QImage& temp = data_label;
                {
//                    std::scoped_lock lock(mtx_data_label);
                    if(data_label.isNull()){
                        return;
                    }
                    temp = data_label.copy();
                }

                double factor_scale_y = (double)temp.height() / label->height();
                double factor_scale_x = (double)temp.width() / label->width();
                auto centerx = pos.x() * factor_scale_x;
                auto centery = pos.y() * factor_scale_y;
                QPointF center(centerx, centery);
                drawRectOrCircle(temp, center, size, QColor(drawValue,drawValue,drawValue));
                if(lastDrawLabel == label){
                    // 绘制两次移动之间的部分

                    auto deltY = center.y() - lastDrawPos.y();
                    auto deltX = center.x() - lastDrawPos.x();
                    auto k = deltY /deltX;
                    auto invK = 1/k;
                    if(std::abs(k) < 1){ // x step
                        for(int delt=1;delt<std::abs(deltX);delt++){
                            auto x = deltX>0?delt:-delt;
                            auto y = x*k;
                            QPointF p(lastDrawPos.x()+x, lastDrawPos.y()+y);
                            drawRectOrCircle(temp, p, size, QColor(drawValue,drawValue,drawValue));
                        }
                    }
                    else{ // y step
                        for(int delt=1;delt<std::abs(deltY);delt++){
                            auto y = deltY>0?delt:-delt;
                            auto x = y * invK;
                            QPointF p(lastDrawPos.x()+x, lastDrawPos.y()+y);
                            drawRectOrCircle(temp, p, size, QColor(drawValue,drawValue,drawValue));
                        }
                    }
                }
//                {
////                    std::scoped_lock lock(mtx_data_label);
//                    data_label = std::move(temp);
//                }
                lastDrawLabel = label;
                lastDrawPos = center;
            };
//            {
//                std::scoped_lock lock(mtx_q_tasks);
//                q_tasks.push(funcDraw);
//                cond_task.notify_one();
//            }
            if(drawValue >= 0){
                funcDraw();
            }


//            qDebug() << "time: " << std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count();
            updateImageView();
            return true;
        }
    }

    if(e->type() == QEvent::Wheel){
        QWheelEvent *we = (QWheelEvent *)e;
        if(we->angleDelta().y() > 0){
            onBtnPrevious();
        }
        else if(we->angleDelta().y() < 0){
            onBtnNext();
        }
        return true;
    }
    if(e->type() == QEvent::KeyPress){
        QKeyEvent* ke = (QKeyEvent*)e;
        // I键和K键调整笔刷大小
        auto value_per_size = double(ui->sldPencilSize->maximum()) / max_size_pencil;
        if(ke->key() == Qt::Key_I){
            ui->sldPencilSize->setValue(ui->sldPencilSize->value() + value_per_size);
            updateCursor();
        }
        else if(ke->key() == Qt::Key_K){
            ui->sldPencilSize->setValue(ui->sldPencilSize->value() - value_per_size);
            updateCursor();
        }
        else if(ke->key() == Qt::Key_Up){
            onBtnPrevious();
        }
        else if(ke->key() == Qt::Key_Down){
            onBtnNext();
        }
        else if((ke->key() == Qt::Key_Z) && (ke->modifiers()&Qt::ControlModifier)){
            restoreBackupLabel();
        }
        else if((ke->key() == Qt::Key_S) && (ke->modifiers()&Qt::ControlModifier)){
            onBtnSave();
        }
        else{
            return false;
        }
        return true;
    }
    return false;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event){
    if(pathDir==""){
        return;
    }
    if(event->mimeData()->hasUrls()){
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event){
    for(auto& url:event->mimeData()->urls()){
        if(!url.isLocalFile()){
            continue;
        }
        auto path = url.toString(QUrl::PreferLocalFile);
        QImage image(path);
        if(image.isNull()){

            continue;
        }
        auto name = url.fileName();
        auto new_path = pathDir + "/" + nameDirImage + "/" + name;
        moveFile(path, new_path, true);
    }
    updateListWidget();
}

MainWindow::~MainWindow()
{
//    is_window_alive = false;
//    thread_task.join();
    delete ui;
}

