#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <queue>
#include <opencv2/opencv.hpp>
class QLabel;
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    const QString nameSettings = "labelSeg.pro";
    const QString nameDirExport = "export";
    const QString nameDirTrash = "trash";
    const QString nameDirImage = "workspace_image";
    const QString nameDirLabel = "workspace_label";
    QString pathDir = "";

    MainWindow(QWidget *parent = nullptr);
    void onChangeLanguage(QAction *action);
    void changeLanguage(const QString &name);
    void onChangeModel(QAction *action);
    void onCreate();
    void onOpen();
    void openProject(const QString& path);

    QImage data_image,  data_saved_label;
    int idx_current;
    int size_pencil=-1;
    const int max_size_pencil=20;
    double transparency_image=-1,transparency_label=-1;
    bool is_fix_size=false;
    QLabel *lblHovering=nullptr;
    const int backup_num_maximun = 100;
    std::deque<QImage> backup_data_label;

    const int FPS_Update = 60;
    //若要启动后台处理线程，以下数据需要互斥访问
    QImage data_label;
//    std::queue<std::function<void()>> q_tasks;
//    std::mutex mtx_data_label,  mtx_q_tasks;
//    std::condition_variable cond_task;
//    std::thread thread_task;
//    std::atomic<bool> is_window_alive = true;
    QTimer timer;
    std::map<QString, cv::dnn::Net> nets;
    void resetData();
    void updateListWidget();
    void updateCursor();
    void updateImageView(bool must=false);
//    void saveLabel();
    void backupLabel();
    void restoreBackupLabel();
    void onBtnPrevious();
    void onBtnNext();
    void onBtnDraw();   // not implemented
    void onBtnWipe();
    void onBtnFixSize(); // not implemented
    void onBtnAutoLabel();
    void onBtnReset();
    void onBtnSave();
    void onBtnRemoveImage();
    void onBtnExport();
    void onItemSelectionChanged();
    bool eventFilter(QObject *, QEvent *);
    ~MainWindow();
signals:
    void signalUpdateImageView(bool must=false);
//public slots:

private:
    Ui::MainWindow *ui;
};


#endif // MAINWINDOW_H
