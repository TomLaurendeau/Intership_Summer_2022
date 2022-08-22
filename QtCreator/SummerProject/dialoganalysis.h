#ifndef DIALOGANALYSIS_H
#define DIALOGANALYSIS_H

#include <QDialog>
#include <QMainWindow>
#include <QVector>
#include <QDebug>
#include <QMessageBox>
#include <QTableWidget>
#include <QString>
#include <QFile>
#include <QFileDialog>
#include <math.h>
#include <QFont>
#include <QBrush>
#include <QColor>
#include <QWidget>
#include <QListWidget>
#include "qcustomplot.h"
#include <QTableWidgetItem>


namespace Ui {
class DialogAnalysis;
}

class DialogAnalysis : public QDialog
{
    Q_OBJECT

public:
    explicit DialogAnalysis(QWidget *parent = 0);
    ~DialogAnalysis();

    void create_vector(QVector<QString> *file_name, QVector<QVector<double>> *colonne0, QVector<QVector<double>> *colonne1, QVector<QVector<double>> *colonne2);

    void find_segmentation(QVector<QVector<double>> *tb, QVector<QVector<double>> *segm);

    double AVG(QVector<double> *tb);

    int DoubleToInt(double a);

    void correction_segm();

    QVector<double> avg_by_seg(QVector<double> *segm, QVector<double> *colonne2);

    void creation_table_to_comparison(QVector<QVector<double>> *segm, QVector<double> *segmentation);

    void tri_and_double(QVector<double> *segmentation);

    void partitioning(QVector<double> *segmentation, QVector<QVector<double>> *average, QVector<QVector<double>> *segm, QVector<QVector<double>> *part);

    void plot(QVector<double> colonne0, QVector<double> colonne2, QString file_name);

    void create_first_row(int column, double value);

    void create_first_column(int row, QString name);

    void put_value_table_partition(double value, int row, int column);

    void create_base_table_partition();

    void create_segmentation_and_avg_total();

    void create_partition();

    void create_table_partition();

private slots:
    void on_btn_select_file_clicked();

    void on_pushButton_clicked();

    void on_clear_clicked();

private:
    Ui::DialogAnalysis *ui;
};

#endif // DIALOGANALYSIS_H
