#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mythread.h"
#include "mythreadplot.h"
#include "mainwindow.h"

// Init values
int rowPP = 0;
int k = 0;
int checkValue = 0;
int comeFromRunButton = 0;
int mon_pid = 0;
int STOP = 0;
int sizeOfArgument = 0;

unsigned int SLEEP_TIME = 10000; // default value
unsigned int TASK_ACTIVE_MS = 10000; // 10s

std::vector<char *> PAPI_EVENTS;
QVector<QString> counters_vector;
QVector<QString> number_counters_vector;
QString Path_selected;
QString Core_selected;
QString SOURCE_DIR;
QString FreeCoreIsSelected;

//var charac

pid_t my_pid;
int status /* unused ifdef WAIT_FOR_COMPLETION */;
int ev_set = PAPI_NULL;
long long values[7];
// initialization the PAPI library
int retval = PAPI_library_init(PAPI_VER_CURRENT);
std::vector<double> instr_ret;
PAPI_option_t opt;
std::vector<double> counter_1; //Counter_1
std::vector<double> counter_2; //Counter_2
std::vector<double> counter_3; //Counter_3
std::vector<double> counter_4; //Counter_4
auto timestamp_start = std::chrono::high_resolution_clock::now();

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    // open dialog to select PROCESSOR / CORE / PATH
    DialogCoreDir dialogCoreDir;
    dialogCoreDir.setModal(true);
    dialogCoreDir.exec();
    Core_selected = dialogCoreDir.get_core();
    if(Core_selected == "FREE"){
        // get the number of logical CPUs.
        int core = sysconf(_SC_NPROCESSORS_CONF);
        FreeCoreIsSelected = QString::number(core);
    }
    Path_selected = dialogCoreDir.get_PATH();
    int  valueContinueApp = dialogCoreDir.ContinueApp().toInt();

    if(valueContinueApp == 0){
        qDebug() << "Quit app";
        QApplication::quit();
        //NEED TO CLOSE MAINWINDOW
    }
    else{ // button "OK" pressed
        this->setWindowTitle("User Interface");

        ui->setupUi(this);

        StartAndStopThread = new Mythread(this);
        connect(StartAndStopThread,&Mythread::startPlot,
                this, &MainWindow::startPlotThread);
        connect(StartAndStopThread, &Mythread::time_exec,
                this, &MainWindow::time_exec);
        connect(StartAndStopThread, &Mythread::runStartAndStop,
                this, &MainWindow::runThread);

        PlotThread = new MyThreadPlot(this);
        connect(PlotThread, &MyThreadPlot::readCounterToPlot,
                this, &MainWindow::ThreadToPlot);

        SOURCE_DIR  = Path_selected + "/Resource_graphs/";

        // read file papi_avail and create file papi_avail.txt
        QProcess::execute( Path_selected + "/executable/papi_avail -a");  // here
        file_path = "";

        // read line by line papi_avail.txt
        int line_count = 0;
        QString Counters[10000];
        QFile file( Path_selected + "/executable/papi_avail.txt");
        if(!file.open(QFile::ReadOnly | QFile::Text)){
               QMessageBox::information(this, "tilte", "No counter available, try to fix the perf_event_paranoid to 0 "
                                                       "in a terninal: ~$ sudo nano /proc/sys/kernel/perf_event_paranoid\n\n"
                                                       "If the problem persists, please check the 'papi_avail.cpp' file");
               return;
        }

        // set listWidget_Available (left list) from papi_avail.txt
        QTextStream in(&file);
        while(!in.atEnd())
        {
                Counters[line_count] = in.readLine();
                line_count++;
        }
        for(int i=0; i<line_count; i++){
            ui->listWidget_Available->addItem(Counters[i]);
        }
        file.close();

        // set current row to 0 for see the first line selected
        ui->listWidget_Available->setCurrentRow(0);

        // set the header name for the argument table
        this->createUI(QStringList() << "*" << "File"
                                     << "Arguments"
                                     << "");

        // set all the Item for the number of core
        ui->comboBox_core->addItem("1");
        ui->comboBox_core->addItem("2");
        ui->comboBox_core->addItem("4");
        ui->comboBox_core->addItem("8");
        ui->comboBox_core->addItem("16");
        ui->comboBox_core->addItem("32");
        ui->comboBox_core->addItem("64");
        ui->comboBox_core->insertSeparator(8);
        ui->comboBox_core->addItem("FREE"); // if free is selected use -> cat proc/cpuinfo | grep 'cpu cores' and return the value !
        ui->comboBox_core->setStyleSheet("combobox-popup: 0;"); // for a maximum number of visible items
        ui->comboBox_core->setCurrentText(Core_selected);

        // set all the Item for the sampling combo box
        ui->comboBox->addItem("DEFAULT");
        for(int i=4; i<50; i++){
            QString SLEEP_TIME_box = QString::number(i*1000);
            ui->comboBox->addItem(SLEEP_TIME_box);
        }
        ui->comboBox->setStyleSheet("combobox-popup: 0;"); // for a maximum number of visible items
        ui->comboBox->setCurrentIndex(0);

        ui->comboBox_timeMax->addItem("DEFAULT");
        for(int i=1; i<59; i++){
            QString TIME_MAX_box = QString::number(i);
            ui->comboBox_timeMax->addItem(TIME_MAX_box);
        }
        for(int i=60; i<601; i=i+30){
            QString TIME_MAX_box = QString::number(i);
            ui->comboBox_timeMax->addItem(TIME_MAX_box);
        }
        ui->comboBox_timeMax->setStyleSheet("combobox-popup: 0;"); // for a maximum number of visible items
        ui->comboBox_timeMax->setCurrentIndex(0);

        ui->progressBar->setOrientation(Qt::Horizontal);
        ui->progressBar->setRange(0, 100); // Let's say it goes from 0 to 100 (%)
        ui->progressBar->setValue(0);
        ui->progressBar->setStyleSheet("QProgressBar::chunk {background: QLinearGradient( x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #00BFFF, stop: 0.5 #729FCF, stop: 1 #4682B4);"
                                       "border-bottom-right-radius: 7px;"
                                       "border-bottom-left-radius: 2px;"
                                       "border: 1px solid black;}");

    }
}

MainWindow::~MainWindow()
{
    /*// clear the current txt files in the folder
    for(int j=0; j<counters_vector.size(); j++){
        if(counters_vector[j]== "PAPI_NULL") break;
        else{
            QString file_name = Path_selected + "/Resource_graphs/" + counters_vector[j] + "_data.txt";
            qDebug() << file_name + " is deleted";
            QDir file;
            file.remove(file_name);
        }
    }*/
    delete ui;
}

/********************************************************************************
 * Name : plot_txt                                                              *
 *                                                                              *
 * Input variable: task_characteristic *output                                  *
 *                 char *counter                                                *
 *                 int number                                                   *
 *                 char *app                                                    *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to write text files that           *
 *                      have previously selected. While reading value of number *
 *                      of instruction / number of cache misses,                *
 *                      it adds each value to its corresponding file            *
 ********************************************************************************/
void MainWindow::plot_txt(task_characteristic *output, char *counter, int number, char *app, char* date_str)
{
    char data_file[130];
    char * source = SOURCE_DIR.toLatin1().data(); // WARNING !!!
    std::ofstream myfile;
    sprintf(data_file, "%s%s/%s_%s_data.txt", source, date_str, app, counter);
    myfile.open(data_file);
    for (int i = 0; i <  (*output).event_counter_sets[0].size(); i ++){
        myfile << std::fixed << i << '\t' <<  (*output).event_counter_sets[0][i] << '\t' << (*output).event_counter_sets[number][i] << "\n";
    }
    myfile.close();
}


/********************************************************************************
 * Name : characterize_program                                                  *
 *                                                                              *
 * Input variable: char **argument                                              *
 *                 char *inCounter1                                             *
 *                 char *inCounter2                                             *
 *                 char *inCounter3                                             *
 *                 char *inCounter4                                             *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: -                                                       *
 ********************************************************************************/
int MainWindow::characterize_program(char **argument, char *inCounter1, char *inCounter2, char *inCounter3, char *inCounter4)
{
    int event_1 = 0, event_2 = 0, event_3 = 0, event_4 = 0;

    ev_set = PAPI_NULL;

    // initialization the PAPI library
    retval = PAPI_library_init(PAPI_VER_CURRENT);

    // create an event
    if ((retval = PAPI_create_eventset(&ev_set)) != PAPI_OK){
        printf("ERROR: PAPI_create_eventset %d: %s\n", retval,
               PAPI_strerror(retval));
        exit(-1);
    }

    //Aassign it to CPU compoment
    if ((retval = PAPI_assign_eventset_component((ev_set), 0)) != PAPI_OK)
    {
        printf("ERROR: PAPI_assign_eventset_component %d: %s\n", retval,
               PAPI_strerror(retval));
        exit(-1);
    }

    if((PAPI_event_name_to_code(("%s",inCounter1), &event_1)!= PAPI_OK) && (strcmp(inCounter1,"PAPI_NULL") == -2)){} //std::cout << std::hex << event_1 << "\n";
    if((PAPI_event_name_to_code(("%s",inCounter2), &event_2)!= PAPI_OK) && (strcmp(inCounter2,"PAPI_NULL") == -2)){} //std::cout << std::hex << event_2 << "\n";
    if((PAPI_event_name_to_code(("%s",inCounter3), &event_3)!= PAPI_OK) && (strcmp(inCounter3,"PAPI_NULL") == -2)){} //std::cout << std::hex << event_3 << "\n";
    if((PAPI_event_name_to_code(("%s",inCounter4), &event_4)!= PAPI_OK) && (strcmp(inCounter4,"PAPI_NULL") == -2)){} //std::cout << std::hex << event_4 << "\n";

    memset(&opt, 0x0, sizeof(PAPI_option_t));
    opt.inherit.inherit = PAPI_INHERIT_ALL;
    opt.inherit.eventset = ev_set;
    if ((retval = PAPI_set_opt(PAPI_INHERIT, &opt)) != PAPI_OK)
    { // most important, the papi inheritance!!!!
        printf("PAPI_set_opt error %d: %s0", retval, PAPI_strerror(retval));
        exit(-1);
    }
    if ((retval = PAPI_add_event(ev_set, PAPI_TOT_INS)) != PAPI_OK) //PAPI_TOT_INS
    {
        printf("failed to retvalattach instructions retired");
        printf("Wrapper create: %s\n", strerror(errno));
    }
    if (((retval = PAPI_add_event(ev_set, event_1)) != PAPI_OK) && (strcmp(inCounter1,"PAPI_NULL") == -2))
    {
        printf("failed to attach event_1 ");
        printf("Wrapper create: %s\n", strerror(errno));
    }
    if (((retval = PAPI_add_event(ev_set, event_2)) != PAPI_OK) && (strcmp(inCounter2,"PAPI_NULL") == -2))
    {
        printf("failed to attach event_2 ");
        printf("Wrapper create: %s\n", strerror(errno));
    }
    if (((retval = PAPI_add_event(ev_set, event_3)) != PAPI_OK) && (strcmp(inCounter3,"PAPI_NULL") == -2))
    {
        printf("failed to attach event_3 ");
        printf("Wrapper create: %s\n", strerror(errno));
    }
    if (((retval = PAPI_add_event(ev_set, event_4)) != PAPI_OK) && (strcmp(inCounter4,"PAPI_NULL") == -2))
    {
        printf("failed to attach event_4 ");
        printf("Wrapper create: %s\n", strerror(errno));
    }
    std::string path = argument[0];
    std::string name;
    size_t sep = path.find_last_of("\\/");
    if (sep != std::string::npos)
            path = path.substr(sep + 1, path.size() - sep - 1);

    size_t dot = path.find_last_of(".");
    if (dot != std::string::npos){
        name = path.substr(0, dot);
        //std::string ext  = path.substr(dot, path.size() - dot);
    }
    else{
        name = path;
        //std::string ext  = "";
    }
    char* name_app = strcpy(new char[name.length() + 1], name.c_str());


    timestamp_start = std::chrono::high_resolution_clock::now();

    //Started thread
    StartAndStopThread->stop = false;
    strcpy(StartAndStopThread->inCounter1,inCounter1);
    strcpy(StartAndStopThread->inCounter2,inCounter2);
    strcpy(StartAndStopThread->inCounter3,inCounter3);
    strcpy(StartAndStopThread->inCounter4,inCounter4);
    strcpy(StartAndStopThread->app,name_app);

    int AAA = 0;
    for(AAA = 0; AAA < sizeOfArgument-6; AAA++){
        StartAndStopThread->argument[AAA] = strdup(argument[AAA]);
    }
    argument[AAA] = NULL;
    STOP = 1;
    StartAndStopThread->start();
    return 0;
}


/********************************************************************************
 * Name : mainProg                                                              *
 *                                                                              *
 * Input variable: char **argv                                                  *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: -                                                       *
 ********************************************************************************/
void MainWindow::mainProg(char **argv)
{
    // initialization
    using namespace std;
    FILE * fp;
    char filepath [60];
        memset(filepath,0,60);
    char counter1[PAPI_MAX_STR_LEN] = "PAPI_NULL";	char counter2[PAPI_MAX_STR_LEN] = "PAPI_NULL"; char counter3[PAPI_MAX_STR_LEN] = "PAPI_NULL"; char counter4[PAPI_MAX_STR_LEN] = "PAPI_NULL";
    // affectation
    strcpy(filepath,argv[1]);
    strcpy(counter1,argv[2]);
    strcpy(counter2,argv[3]);
    strcpy(counter3,argv[4]);
    strcpy(counter4,argv[5]);

    // open file with the filepath
    fp = fopen(filepath, "r");
    if (fp == NULL)
    {
        qDebug()<< "Could not read file";
        exit(EXIT_FAILURE);
    }
    unsigned int size = sizeOfArgument - 5;
    char ** argument_exe = (char**) malloc((size) * sizeof(char*));
    for(unsigned int i=0; i < (size) ; ++i ){
        argument_exe[i] = (char*) malloc(100*sizeof(char));
    }
    strcpy(argument_exe[0], argv[1]);
    for(unsigned int i=1; i<size-1; i++){
         strcpy(argument_exe[i], argv[5+i]);
    }
    argument_exe[size-1] = NULL;
    characterize_program(argument_exe, (char*)counter1, (char*)counter2, (char*)counter3, (char*)counter4);
    for(unsigned int i = 0; i < size; i++)
        free(argument_exe[i]);
    free(argument_exe);
}



/********************************************************************************
 * Name : create_graph_folder                                                   *
 *                                                                              *
 * Input variable:                                                              *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: create a folder in the computer at a specific place     *
 *                      with date hour                                          *
 ********************************************************************************/
char *MainWindow::create_graph_folder()
{
    char buffer_str [80];
    char * date_str;
    date_str = (char*)malloc(sizeof(char)*80);
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    const char* source = SOURCE_DIR.toLocal8Bit().data();
    sprintf(date_str, "graph_%d-%02d-%02d_%02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    sprintf(buffer_str, "mkdir %s%s", source, date_str);
    system(buffer_str);
    return date_str;
}


/********************************************************************************
 * Name : createUI                                                              *
 *                                                                              *
 * Input variable: QStringList &headers                                         *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to create the tablewidget in the   *
 *                      arguement part                                          *
 ********************************************************************************/
void MainWindow::createUI(const QStringList &headers)
{
    //set the UI environnement
    ui->tableWidget->setColumnCount(4);
    ui->tableWidget->setShowGrid(true);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget->setHorizontalHeaderLabels(headers);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

    //set ColumnWidth
    ui->tableWidget->setColumnWidth(0,11);
    ui->tableWidget->setColumnWidth(1,470);
    ui->tableWidget->setColumnWidth(2,300);
    ui->tableWidget->setColumnWidth(3,20);
}


/********************************************************************************
 * Name : on_pushButton_add_clicked                                             *
 *                                                                              *
 * Input variable: connect the signal from the push button "add" to this slot   *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to add counter from "available     *
 *                      counters" list to "selected counters" list              *
 ********************************************************************************/
void MainWindow::on_pushButton_add_clicked()
{
    int already_exist;
    int row = ui->listWidget_selected->count();
    int row_Available = ui->listWidget_Available->count();
    if(ui->listWidget_Available->currentItem())
    {
        QString selected_item = ui->listWidget_Available->currentItem()->text();
        QListWidgetItem *add_counter = new QListWidgetItem(selected_item);
        if(row == 0){
            ui->listWidget_selected->addItem(add_counter);
            ui->listWidget_selected->setCurrentRow(0);

        }
        else{
            for(int i=0; i<row; i++){
                QString otherItem = ui->listWidget_selected->item(i)->text();
                if(selected_item == otherItem){
                    already_exist = 1;
                    break;
                }
                else
                    already_exist = 0;
            }
            if (already_exist == 0){
                ui->listWidget_selected->addItem(add_counter);
                ui->listWidget_selected->setCurrentRow(row);
            }
        }
        rowPP = ui->listWidget_Available->currentRow()+1;
        ui->listWidget_Available->setCurrentRow(rowPP % row_Available);
        on_listWidget_Available_clicked();
        on_listWidget_selected_clicked();

    }
    else{
        QMessageBox::warning(this, "WARNING", "No Counter Selected");
    }
}


/********************************************************************************
 * Name : on_pushButton_delete_clicked                                          *
 *                                                                              *
 * Input variable: connect the signal from the push button "delete" to this     *
 *                 slot                                                         *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to remove counter from             *
 *                      "selected counters" list                                *
 ********************************************************************************/
void MainWindow::on_pushButton_delete_clicked()
{
    delete ui->listWidget_selected->currentItem();
    int row = ui->listWidget_selected->count();
    if(row == 0){
        ui->plainTextEdit_Selected_Counters->clear();
    }
    else{
        QString Item_Counter = ui->listWidget_selected->currentItem()->text();
        counter_description(0, Item_Counter);
    }

}


/********************************************************************************
 * Name : on_pushButton_delete_all_clicked                                      *
 *                                                                              *
 * Input variable: connect the signal from the push button "delete all" to this *
 *                 slot                                                         *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to remove counters in              *
 *                      "selected counters" list                                *
 ********************************************************************************/
void MainWindow::on_pushButton_delete_all_clicked()
{
    // clear right list and the description of the counter
    ui->listWidget_selected->clear();
    ui->plainTextEdit_Available_Counters->clear();
    ui->plainTextEdit_Selected_Counters->clear();
    ui->listWidget_selected->setCurrentRow(0);
    ui->listWidget_Available->setCurrentRow(-1);
    rowPP = 0;
}


/********************************************************************************
 * Name : on_pushButton_delete_all_clicked                                      *
 *                                                                              *
 * Input variable: int value                                                    *
 *                 QString Item_Counter                                         *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to show description of the         *
 *                      selected counters regardless of the list                *
 ********************************************************************************/
void MainWindow::counter_description(int value, QString Item_Counter)
{
    // set description of the differents counters from a file .txt to a plaintext
    QFile file( Path_selected + "/QtCreator/SummerProject/Explanatory/PAPI_EVENT_LIST.txt");
    if(!file.open(QFile::ReadOnly | QFile::Text)){
       QMessageBox::warning(this, "title", "file not open");
    }
    QTextStream in(&file);
    QString text;
    QString line = in.readLine();
    do{
        line = in.readLine();
        if (line.contains(Item_Counter, Qt::CaseSensitive)) {
            text = line;
        }
    }
    while(!line.isNull());
    if (value == 1)
        ui->plainTextEdit_Available_Counters->setPlainText("PAPI preset events:\n\n"+ text);
    if (value == 0)
        ui->plainTextEdit_Selected_Counters->setPlainText("PAPI preset events:\n\n" + text);
    file.close();
}


/********************************************************************************
 * Name : on_listWidget_Available_clicked                                       *
 *                                                                              *
 * Input variable: connect the signal from the "listWidget_Available" when      *
 *                 there was a click to this slot                               *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to define which of the counters is *
 *                      selected in "available counters" list to show its       *
 *                      description                                             *
 ********************************************************************************/
void MainWindow::on_listWidget_Available_clicked()
{
    QString Item_Counter = ui->listWidget_Available->currentItem()->text();
    counter_description(1, Item_Counter);
}


/********************************************************************************
 * Name : on_listWidget_selected_clicked                                        *
 *                                                                              *
 * Input variable: connect the signal from the "listWidget_selected"  when      *
 *                 there was a click to this slot                               *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to define which of the counters is *
 *                      selected in "selected counters" list to show its        *
 *                      description                                             *
 ********************************************************************************/
void MainWindow::on_listWidget_selected_clicked()
{
    QString Item_Counter = ui->listWidget_selected->currentItem()->text();
    counter_description(0, Item_Counter);
}


/********************************************************************************
 * Name : on_listWidget_Available_doubleClicked                                 *
 *                                                                              *
 * Input variable: connect the signal from the "listWidget_Available" when      *
 *                 there was a double click to this slot                        *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to add the counter selected from   *
 *                      "available counters" list to "selected counters" list   *
 *                      and show its description                                *
 ********************************************************************************/
void MainWindow::on_listWidget_Available_doubleClicked()
{
    //add the double clicked from the available list to the selected list
    int already_exist;
    int row = ui->listWidget_selected->count();
    if(ui->listWidget_Available->currentItem())
    {
        QString selected_item = ui->listWidget_Available->currentItem()->text();
        QListWidgetItem *add_counter = new QListWidgetItem(selected_item);
        if(row == 0){
            ui->listWidget_selected->addItem(add_counter);
            ui->listWidget_selected->setCurrentRow(0);
        }
        else{
            for(int i=0; i<row; i++){
                QString otherItem = ui->listWidget_selected->item(i)->text();
                if(selected_item == otherItem){
                    already_exist = 1;
                    break;
                }
                else
                    already_exist = 0;
            }
            if (already_exist == 0){
                ui->listWidget_selected->addItem(add_counter);
                ui->listWidget_selected->setCurrentRow(row);
            }
        }
    }
    on_listWidget_selected_clicked();
}


/********************************************************************************
 * Name : on_actionExit_triggered                                               *
 *                                                                              *
 * Input variable: connect the signal from the "exit" button in the menu bar to *
 *                 this slot                                                    *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to close the application           *
 ********************************************************************************/
void MainWindow::on_actionExit_triggered()
{
    QApplication::quit();
}


/********************************************************************************
 * Name : on_actionSave_Profil_triggered                                        *
 *                                                                              *
 * Input variable: connect the signal from the "save" action button in the menu *
 *                 bar to this slot                                             *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to save the counters selected in   *
 *                      file txt                                                *
 ********************************************************************************/
void MainWindow::on_actionSave_Profil_triggered()
{
    //save counters selected in a file .txt
    int row = ui->listWidget_selected->count();
    QString counter_selected = "";
    if(row != 0){
        for(int i=0; i<row; i++){
            counter_selected = counter_selected + ui->listWidget_selected->item(i)->text() + "\n";
        }
        QMessageBox::StandardButton reply = QMessageBox::question(this,"QUESTION", "You'll save: \n" + counter_selected,
                              QMessageBox::Yes | QMessageBox::No);
         if (reply == QMessageBox::Yes)
         {
             QString file_name = QFileDialog::getSaveFileName(this, "Open file", Path_selected + "/QtCreator/SummerProject/Profile");
             QFile file(file_name);
             file_path = file_name;
             if(!file.open(QFile::WriteOnly | QFile::Text)){
                    QMessageBox::warning(this, "WARNING", "The file wasn't saved");
                    return;
             }
             QTextStream out(&file);
             out << counter_selected;
             file.flush();
             file.close();
         }
    }
    else{
        QMessageBox::warning(this, "WARNING", "No counter selected \n"
                                              "Can't saved, please retry");
    }
}


/********************************************************************************
 * Name : on_actionDelete_profil_triggered                                      *
 *                                                                              *
 * Input variable: connect the signal from the "delete" action button in the    *
 *                 menu bar to this slot                                        *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to remove file saved               *
 ********************************************************************************/
void MainWindow::on_actionDelete_profil_triggered()
{
    //delete profil
    QString file_name = QFileDialog::getOpenFileName(this, "Open file", Path_selected + "/QtCreator/SummerProject/Profile");
    QDir dir;
    dir.remove(file_name);
}


/********************************************************************************
 * Name : on_pushButton_all_clicked                                             *
 *                                                                              *
 * Input variable: connect the signal from the push button "select all" to this *
 *                 slot                                                         *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to add all counters from           *
 *                      "available counters" list to "selected counters" list   *
 ********************************************************************************/
void MainWindow::on_pushButton_all_clicked()
{
    int row_Available = ui->listWidget_Available->count();
    ui->listWidget_selected->clear();
    for(int i=0; i<row_Available; i++){
            QString selected_item = ui->listWidget_Available->item(i)->text();
            QListWidgetItem *add_counter = new QListWidgetItem(selected_item);
            ui->listWidget_selected->addItem(add_counter);
            ui->listWidget_selected->setCurrentRow(i);
        }
    on_listWidget_selected_clicked();

}


/********************************************************************************
 * Name : on_actionDelete_profil_triggered                                      *
 *                                                                              *
 * Input variable: connect the signal from the "open" action button in the      *
 *                 menu bar to this slot                                        *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to open file saved                 *
 ********************************************************************************/
void MainWindow::on_actionOpen_profil_triggered()
{
    //read .txt and add the selected counters
    int line_count=0;
    QString line[100];

    QString file_name = QFileDialog::getOpenFileName(this, "Open file", Path_selected + "/QtCreator/SummerProject/Profile");
    QFile file(file_name);
    if(!file.open(QFile::ReadOnly | QFile::Text)){
           QMessageBox::information(this, "tilte", "file not open");
           return;
    }
    ui->listWidget_selected->clear();
    ui->plainTextEdit_Available_Counters->clear();
    ui->plainTextEdit_Selected_Counters->clear();


    QTextStream in(&file);
    while( !in.atEnd())
    {
        line[line_count]=in.readLine();
        line_count++;
    }
   // QString text = in.readAll();
    for(int i=0; i<line_count; i++){
        QString selected_item = line[i];
        QListWidgetItem *add_counter = new QListWidgetItem(selected_item);
        ui->listWidget_selected->addItem(add_counter);
        ui->listWidget_selected->setCurrentRow(i);
    }
    file.close();

    //set the cursor at the first row and show the description
    ui->listWidget_selected->setCurrentRow(0);
    on_listWidget_selected_clicked();
    ui->listWidget_Available->setCurrentRow(0);
    on_listWidget_Available_clicked();
}


/********************************************************************************
 * Name : on_actionAbout_triggered                                              *
 *                                                                              *
 * Input variable: connect the signal from the "about" action button in the     *
 *                 menu bar to this slot                                        *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to show a message box              *
 ********************************************************************************/
void MainWindow::on_actionAbout_triggered()
{
    QString about_text;
    about_text = "Auther : Tom Laurendeau \n";
    about_text += "Collaborator : Regis Jubeau \n";
    about_text += "Tutors : Mr and Mrs Seceleanu \n";
    QMessageBox::about(this, "Information", about_text);
}


/********************************************************************************
 * Name : on_actionSave_Argument_triggered                                      *
 *                                                                              *
 * Input variable: connect the signal from the "Save" action button in the      *
 *                 menu bar (arguement) to this slot                            *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to save argument profile          *
 ********************************************************************************/
void MainWindow::on_actionSave_Argument_triggered()
{
    //save counters selected in a file .txt
    int row = ui->tableWidget->rowCount();
    int check = 0;
    if(row == 0){
            QMessageBox::warning(this, "WARNING", "No argument selected \n"
                                                  "Can't saved, please retry");
        return;
    }
    for(int i=0; i<row; i++)
        if(ui->tableWidget->item(i,0)->checkState())
            check++;

    QMessageBox::StandardButton reply = QMessageBox::question(this,"QUESTION", "You'll save: \n" + QString::number(check) + " argument(s)",
                          QMessageBox::Yes | QMessageBox::No);
     if (reply == QMessageBox::Yes)
     {

         QString file_name = QFileDialog::getSaveFileName(this, "Open file", Path_selected + "/QtCreator/SummerProject/Arguments");
         QFile file(file_name);
         file_path = file_name;
         if(!file.open(QFile::WriteOnly | QFile::Text)){
                QMessageBox::warning(this, "WARNING", "The file wasn't saved");
                return;
         }
         for(int i=0; i<row; i++){
             if(ui->tableWidget->item(i,0)->checkState()){
                 int currentRow = i;
                 QLabel *lineEdit = (QLabel*)ui->tableWidget->cellWidget(currentRow,1);
                 QString full_Path = lineEdit->text();
                 QString argument = ui->tableWidget->item(currentRow,2)->text();
                 QTextStream out(&file);
                 out << full_Path + "\t" + argument + "\n";
             }
         }
         file.flush();
         file.close();
     }
     else
         return;
}


/********************************************************************************
 * Name : on_actionDelete_Argument_triggered                                    *
 *                                                                              *
 * Input variable: connect the signal from the "Delate" action button in the    *
 *                 menu bar (arguement) to this slot                            *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to delete argument profile         *
 ********************************************************************************/
void MainWindow::on_actionDelete_Argument_triggered()
{
    //delete profil
    QString file_name = QFileDialog::getOpenFileName(this, "Open file", Path_selected + "/QtCreator/SummerProject/Arguments");
    QDir dir;
    dir.remove(file_name);
}


/********************************************************************************
 * Name : on_actionDelete_Argument_triggered                                    *
 *                                                                              *
 * Input variable: connect the signal from the "open" action button in the      *
 *                 menu bar (arguement) to this slot                            *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to open argument profile           *
 ********************************************************************************/
void MainWindow::on_actionOpen_Argument_triggered()
{
    //read .txt and add the selected counters
    int line_count=0;
    QString line[100];

    QString file_name = QFileDialog::getOpenFileName(this, "Open file", Path_selected + "/QtCreator/SummerProject/Arguments");
    QFile file(file_name);
    if(!file.open(QFile::ReadOnly | QFile::Text)){
           QMessageBox::information(this, "tilte", "file not open");
           return;
    }

    QTextStream in(&file);
    while(!in.atEnd())
    {
        QString line = in.readLine();
        QStringList words = line.split("\t");
        QString fullpath = words[0];
        QString arguments = words[1];
        ui->tableWidget->insertRow(0);
        QLabel *lineedit = new QLabel();
        lineedit->setText(fullpath);
        ui->tableWidget->setCellWidget(0, 1, lineedit);

        QTableWidgetItem *item_checkbox = new QTableWidgetItem();
        item_checkbox->data(Qt::CheckStateRole);
        item_checkbox->setCheckState(Qt::Checked);

        QTableWidgetItem *argument = new QTableWidgetItem();
        argument->data(Qt::EditRole);
        argument->setText(arguments);

        QPushButton *button_file_argument = new QPushButton();
        button_file_argument->setText("...");
        connect(button_file_argument, SIGNAL(clicked(bool)), this, SLOT(onClicked()));

        ui->tableWidget->setItem(0,0, item_checkbox);
        ui->tableWidget->setItem(0, 2, argument);
        ui->tableWidget->setCellWidget(0, 3, button_file_argument);
    }
    file.close();
}


/********************************************************************************
 * Name : validateCounters                                                      *
 *                                                                              *
 * Input variable: -                                                            *
 *                                                                              *
 * Output variable: tooMushCounters                                             *
 *                                                                              *
 * Summary of function: This function allows to validate if there are less 4    *
 *                      counters selected put this counter(s) in a vector and   *
 *                      return 0 else return 1 or 2 if no counter was selected  *
 ********************************************************************************/
int MainWindow::validateCounters()
{
    int row = ui->listWidget_selected->count();
    int row_avail = ui->listWidget_Available->count();
    counters_vector.clear();
    number_counters_vector.clear();
    if(row<5 && row !=0){
        for(int i=0; i<row;i++)
        {
            QString counter = ui->listWidget_selected->item(i)->text();
              counters_vector.append(counter);//put in some vector the selected counters
              for(int j=0; j<row_avail; j++)
                if(counter == (ui->listWidget_Available->item(j)->text())){
                    number_counters_vector.append(QString::number(j));
                }
        }
        while(counters_vector.size()<4){
            counters_vector.append("PAPI_NULL");
            number_counters_vector.append("-1");
        }
        return 0;
    }
    else{
        if(row == 0)
            return 2;
        else
            return 1;
    }
}


/********************************************************************************
 * Name : on_pushButton_File_Dialogue_New_clicked                               *
 *                                                                              *
 * Input variable: connect the signal from the push button "New" to this        *
 *                 slot                                                         *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to create a new row in the         *
 *                      "tableWidget"                                             *
 *                      open file dialog to select the application who will run *
 *                      open file dialog to select the argument for the         *
 *                      application                                             *
 ********************************************************************************/
void MainWindow::on_pushButton_File_Dialogue_New_clicked()
{
    int newFile = 1;
    int newFileArg = 1;

   //Open file to get the full path
    QString file_name = QFileDialog::getOpenFileName(this, "Select an application", QDir::homePath());
    QFile file(file_name);
    if(!file.open(QFile::ReadOnly | QFile::Text)){
        newFile = 0;
    }
    if(newFile){
        // Insert row
        ui->tableWidget->insertRow(0);

        // Create a String, which will serve as a name in a line edit
        //QString name_apllication = file_name.section("/", -1,-1);

        // Create an element, which will serve as a line edit
        QLabel *lineedit = new QLabel();
        lineedit->setText(file_name);
        //Set the edit line in the column 2
        ui->tableWidget->setCellWidget(0, 1, lineedit);

        file.close();

        // Create an element, which will serve as a checkbox
        QTableWidgetItem *item_checkbox = new QTableWidgetItem();
        item_checkbox->data(Qt::CheckStateRole);
        item_checkbox->setCheckState(Qt::Checked);

        // Create an element, which will serve as a line edit
        QTableWidgetItem *argument = new QTableWidgetItem();
        argument->data(Qt::EditRole);

        //Open file to get the full path argument
        QString file_name_argument = QFileDialog::getOpenFileName(this, "Select an Argument", file_name);
        QFile file(file_name_argument);
        if(!file.open(QFile::ReadOnly | QFile::Text)){
         newFileArg = 0;
        }
        if(newFileArg)
            argument->setText(file_name_argument);
        else
            argument->setText("");

        // Create an element, which will serve as a push buton
        QPushButton *button_file_argument = new QPushButton();
        button_file_argument->setText("...");
        connect(button_file_argument, SIGNAL(clicked(bool)), this, SLOT(onClicked()));

        // Set the checkbox in the column 1
        ui->tableWidget->setItem(0,0, item_checkbox);

        //Set the edit line in the column 4
        ui->tableWidget->setItem(0, 2, argument);

        //Set the edit line in the column 4
        ui->tableWidget->setCellWidget(0, 3, button_file_argument);
    }
}


/********************************************************************************
 * Name : on_pushButton_File_Dialogue_Delete_clicked                            *
 *                                                                              *
 * Input variable: connect the signal from the push button "Delete" to this     *
 *                 slot                                                         *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to remove the selected row from    *
 *                      the "tableWidget"                                       *
 ********************************************************************************/
void MainWindow::on_pushButton_File_Dialogue_Delete_clicked()
{
    int i = ui->tableWidget->currentRow();
    if(i != -1){
        ui->tableWidget->removeRow(i);
    }
}


/********************************************************************************
 * Name : on_comboBox_core_currentIndexChanged                                  *
 *                                                                              *
 * Input variable: connect the signal from the "comboBox_core" then the index   *
 *                 chqnge to this slot                                          *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to set the different comboBox in   *
 *                      "tablewidget" with number of core                       *
 ********************************************************************************/
void MainWindow::on_comboBox_core_currentIndexChanged()
{
    /*for(int j=0; j<ui->tableWidget->rowCount(); j++){

        QComboBox *combobox = new QComboBox();
        QString OS_core = ui->comboBox_core->currentText();
        int core;
        if(OS_core == "FREE"){
            core = FreeCoreIsSelected.toInt();
        }
        else
            core = OS_core.toInt();

        for(int i=0; i<core; i++){
                combobox->addItem(QString::number(i));
        }
        //Set the combo box in the column 3
        ui->tableWidget->setCellWidget(j, 2, combobox);
        combobox->setStyleSheet("combobox-popup: 0;");
    }*/
}


/********************************************************************************
 * Name : on_tableWidget_doubleClicked                                          *
 *                                                                              *
 * Input variable: connect the signal from the "tableWidget" when               *
 *                 there was a double click to this slot                        *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to add a new path application when *
 *                      double clic on the right cell                           *
 ********************************************************************************/
void MainWindow::on_tableWidget_doubleClicked()
{
    int currentRow = ui->tableWidget->currentRow();
    int newFileName = 1;
    QLabel *lineEdit = (QLabel*)ui->tableWidget->cellWidget(currentRow,1);
    QString pathClick = lineEdit->text();
    if(ui->tableWidget->selectionModel()->currentIndex().column() == 1){ // == 1 && !(ui->tableWidget->focusWidget())
        QString file_name = QFileDialog::getOpenFileName(this, "Open a file", pathClick);
        QFile file(file_name);
        if(!file.open(QFile::ReadOnly | QFile::Text)){
          // QMessageBox::warning(this, "title", "file not change");
           newFileName = 0;
        }
        if (newFileName){
            // Create a String, which will serve as a name in a line edit
            //QString name_apllication = file_name.section("/", -1,-1);

            // Create an element, which will serve as a line edit
            QLabel *lineedit = new QLabel();
            lineedit->setText(file_name);
            //Set the edit line in the column 2
            ui->tableWidget->setCellWidget(currentRow, 1, lineedit);
        }
    }
}


/********************************************************************************
 * Name : onClicked                                                             *
 *                                                                              *
 * Input variable: connect the signal from the "tableWidget" when               *
 *                 there was a click to this slot                               *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to et the argument (line edit)     *
 *                      with the new path                                       *
 ********************************************************************************/
void MainWindow::onClicked()
{
    int newFileName = 1;
    QPushButton *w = qobject_cast<QPushButton *>(QObject::sender());

    if(w){
        int row = getCurrentRow();
        QString file_name = QFileDialog::getOpenFileName(this, "Open a file", QDir::homePath());
        QFile file(file_name);
        if(!file.open(QFile::ReadOnly | QFile::Text)){
            newFileName = 0;
        }
        if (newFileName){
            // Create a String, which will serve as a name in a line edit
            //QString name_arg = file_name.section("/", -1,-1);

            // Create an element, which will serve as a line edit
            QTableWidgetItem *lineedit = new QTableWidgetItem();
            lineedit->data(Qt::EditRole);
            lineedit->setText(file_name);

            //Set the edit line in the column 2
            ui->tableWidget->setItem(row, 2, lineedit);
        }
    }
}


/********************************************************************************
 * Name : getCurrentRow                                                         *
 *                                                                              *
 * Input variable: -                                                            *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to return the current Row of the   *
 *                      focus Widget else return -1                             *
 ********************************************************************************/
int MainWindow::getCurrentRow()
{
    for(int i=0; i<ui->tableWidget->rowCount(); i++){
        for(int j=0; j<ui->tableWidget->columnCount(); j++) {
            if(ui->tableWidget->cellWidget(i,j) == ui->tableWidget->focusWidget()) {
                return i;
            }
        }
    }
    return -1;
}


/********************************************************************************
 * Name : on_pushButton_Arguments_Check_clicked                                 *
 *                                                                              *
 * Input variable: connect the signal from the push button "Check" when         *
 *                 there was a click to this slot                               *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to ckeck if there is a apllication *
 *                      selected and if numbers of counters are correct         *
 ********************************************************************************/
void MainWindow::on_pushButton_Arguments_Check_clicked()
{
    int currentRow = -1;
    int checkState = 0;
    int row = ui->tableWidget->rowCount();
    int tooMushCounters = validateCounters();
    if(row != 0 && number_counters_vector.size() != 0){
        for(int i=0; i<row; i++){
            if(ui->tableWidget->item(i,0)->checkState()){
                currentRow = i;
                checkState++;
            }
        }

        if(checkState == 1 && currentRow != -1){
            QLabel *lineEdit = (QLabel*)ui->tableWidget->cellWidget(currentRow,1);
            QString full_Path = lineEdit->text();
            QString argument = ui->tableWidget->item(currentRow,2)->text();
            //QString number_counters = "";
            QString StringListVectorCounters = "";
            for(int k=0; k<number_counters_vector.count(); k++)
            {
                if(counters_vector[k]!= "PAPI_NULL")
                //number_counters += number_counters_vector[k] + " ";
                StringListVectorCounters += counters_vector[k] + " ";
            }
            if(!comeFromRunButton)QMessageBox::information(this, "Checking", "Counter selected : " + StringListVectorCounters
                                                                     +"\nFull Path Application: " + full_Path
                                                                     + "\nArgument selected: " + argument);
             checkValue = 1;
        }
        else{
            if(checkState == 0)
                QMessageBox::warning(this, "Warning", "No application selected");
            else
                QMessageBox::warning(this,"Warning","Too mush application selected");

            checkValue = 0;
        }
    }
    else {
        checkValue = 0;
        if(row == 0 && number_counters_vector.size() != 0 && tooMushCounters == 0)
            QMessageBox::warning(this,"WARNING","No Application in the list,\n please retry when you have added at least one ");

        else if(row != 0 && number_counters_vector.size() == 0 && tooMushCounters == 2)
            QMessageBox::warning(this,"WARNING","No Vectors selected,\nPlease retry when you have added at least one ");

        else if(tooMushCounters == 1 && row == 0 && number_counters_vector.size() != 0)
            QMessageBox::warning(this, "WARNING", "Too much Counters selected (max: 4)\nNo Application in the list,\nPlease retry when you have added at least one and delete some counter ");

        else if(tooMushCounters == 1 && row != 0 && number_counters_vector.size() != 0)
            QMessageBox::warning(this, "WARNING", "Too much Counters selected (max: 4),\nPlease delete some counter");

        else
            QMessageBox::warning(this,"WARNING","No Application in the list,\nNo Vectors selected,\nPlease retry when you have added at least one of each");
    }
}


/********************************************************************************
 * Name : on_pushButton_Arguments_Run_clicked                                   *
 *                                                                              *
 * Input variable: connect the signal from the push button "Run" when           *
 *                 there was a click to this slot                               *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to run application if function     *
 *                      check is validate and do application with its arguments *
 *                      and counters selected                                   *
 ********************************************************************************/
void MainWindow::on_pushButton_Arguments_Run_clicked()
{
    on_pushButton_Plot_Clear_clicked();
    ui->progressBar->reset();
    int currentRow = -1;
    int checkState = 0;
    int row = ui->tableWidget->rowCount();
    comeFromRunButton = 1;
    on_pushButton_Arguments_Check_clicked();
    comeFromRunButton = 0;
    if(row != 0 && number_counters_vector.size() != 0){
        for(int i=0; i<row; i++){
            if(ui->tableWidget->item(i,0)->checkState()){
                currentRow = i;
                checkState++;
            }
        }
        if(checkState == 1 && currentRow != -1 && checkValue == 1){
            QLabel *lineEdit = (QLabel*)ui->tableWidget->cellWidget(currentRow,1);
            std::string utf8_full_Path = lineEdit->text().toUtf8().constData();
                const char* full_Path = utf8_full_Path.c_str();
            std::string utf8_counter0 = counters_vector[0].toUtf8().constData();
                const char* counter0 = utf8_counter0.c_str();
            std::string utf8_counter1 = counters_vector[1].toUtf8().constData();
                const char* counter1 = utf8_counter1.c_str();
            std::string utf8_counter2 = counters_vector[2].toUtf8().constData();
                const char* counter2 = utf8_counter2.c_str();
            std::string utf8_counter3 = counters_vector[3].toUtf8().constData();
                const char* counter3 = utf8_counter3.c_str();
            QString utf8_argument = ui->tableWidget->item(currentRow,2)->text();
                QStringList argumentList = utf8_argument.split(' ', QString::SkipEmptyParts);

            char ** argumentProg = (char**) malloc((7+argumentList.size()) * sizeof(char*));
            for(int i=0; i < 7+argumentList.size() ; ++i )
            {
                argumentProg[i] = (char*) malloc(100*sizeof(char));
            }
            strcpy(argumentProg[0], "a.out");
            strcpy(argumentProg[1], full_Path);
            strcpy(argumentProg[2], counter0);
            strcpy(argumentProg[3], counter1);
            strcpy(argumentProg[4], counter2);
            strcpy(argumentProg[5], counter3);
            int k=0;
            for(k=0; k<argumentList.size(); k++){
                std::string buffer = argumentList[k].toUtf8().constData();
                const char* argList = buffer.c_str();
                strcpy(argumentProg[k+6], argList);
            }
            argumentProg[k+6] = NULL;
            sizeOfArgument = 7+argumentList.size();
            mainProg(argumentProg);
            for(int i = 0; i < 7+argumentList.size(); i++)
                free(argumentProg[i]);
            free(argumentProg);
        }
    }
}


/********************************************************************************
 * Name : createPlot                                                            *
 *                                                                              *
 * Input variable: QVector<int> *x_                                             *
 *                 QVector<float> *y_                                           *
 *                 QString file_name                                            *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to create a new plot in the row 0  *
 ********************************************************************************/
void MainWindow::createPlot(QVector<int> *x_, QVector<float> *y_, QString file_name)
{
        ui->tableWidget_2->insertRow(0);
        ui->tableWidget_2->setColumnCount(1);
        ui->tableWidget_2->setShowGrid(true);
        ui->tableWidget_2->setSelectionMode(QAbstractItemView::SingleSelection);
        ui->tableWidget_2->setHorizontalHeaderLabels(QStringList() << "plot" );
        ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);
        ui->tableWidget_2->setRowHeight(0, 295);

        QCustomPlot *customPlot1 = new QCustomPlot(QCUSTOMPLOT_H);

        customPlot1->clearGraphs();

        // generate some points of data (y0 for first, y1 for second graph):
        QVector<double> x(x_->size()), y(y_->size());// y2(x_->size()); // initialize with entries 0..100

        for (int i=0; i<x_->size(); i++)
        {
          x[i] = x_->at(i);
          y[i] = y_->at(i);
        }

        // create graph and assign data to it:
        customPlot1->addGraph();

        customPlot1->graph(0)->setData(x, y);

        // let the ranges scale themselves so graph 0 fits perfectly in the visible area:
        customPlot1->graph(0)->rescaleAxes();

        // give the axes some labels
        customPlot1->xAxis->setLabel("Number of values");
        customPlot1->yAxis->setLabel("Cache misses");

        //legend
        QFont legendFont = font();  // start out with MainWindow's font..
        customPlot1->legend->setVisible(true);
        customPlot1->legend->setFont(legendFont);
        customPlot1->legend->setBrush(QBrush(QColor(255,255,255,230)));

        customPlot1->graph(0)->setLineStyle(QCPGraph::lsNone);
        customPlot1->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssPlus, 3));
        // by default, the legend is in the inset layout of the main axis rect. So this is how we access it to change legend placement:
        customPlot1->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignBottom|Qt::AlignRight);

        // add two new graphs and set their look:
        customPlot1->graph(0)->setPen(QPen(Qt::red)); // line color blue for first graph
        customPlot1->graph(0)->setName("Caches misses " + file_name);

        // Allow user to drag axis ranges with mouse, zoom with mouse wheel and select graphs by clicking:
        customPlot1->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
        customPlot1->replot();

        ui->tableWidget_2->setCellWidget(0, 0, customPlot1);
}


/********************************************************************************
 * Name : readFilesPlots                                                        *
 *                                                                              *
 * Input variable: QString file_name                                            *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to read a txt file for a future    *
 *                      plot                                                    *
 ********************************************************************************/
void MainWindow::readFilesPlots(QString file_name)
{
    //read .txt and values in vectors
    int i = 0;
    QVector<int> iTime;
    QVector<float> cacheMisses, numberInstruction;
    bool ok;
    QFile file(file_name);
    if(!file.open(QFile::ReadOnly | QFile::Text)){
           //QMessageBox::information(this, "tilte", "file not open");
           return;
    }
    QTextStream in(&file);
    while( !in.atEnd())
    {
        QString line =in.readLine();
        QStringList split = line.split("\t");
        foreach(QString word, split)
        {
            if (i == 0)
            {
                iTime.append(word.toInt(&ok));
                i=1;
            }
            else if (i == 1)
            {
                numberInstruction.append(word.toFloat(&ok));
                i=2;
            }
            else if (i == 2)
            {
                cacheMisses.append(word.toFloat(&ok));
                i = 0;
            }
        }
    }
    file.close();
    createPlot(&iTime, &cacheMisses, file_name.section("/", -1,-1));
}


/********************************************************************************
 * Name : on_pushButton_Plot_To_File_clicked                                    *
 *                                                                              *
 * Input variable: connect the signal from the push button "Plot_To_file" when  *
 *                 there was a click to this slot                               *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to select a file and plot it       *
 ********************************************************************************/
void MainWindow::on_pushButton_Plot_To_File_clicked()
{
    on_pushButton_Plot_Clear_clicked();

    QString file_name = QFileDialog::getOpenFileName(this, "Open file", Path_selected + "/Resource_graphs_saved");
    QFile file(file_name);
    if(!file.open(QFile::ReadOnly | QFile::Text)){
           QMessageBox::information(this, "tilte", "file not open");
           return;
    }
    readFilesPlots(file_name);
}


/********************************************************************************
 * Name : on_pushButton_savePlot_clicked                                        *
 *                                                                              *
 * Input variable: connect the signal from the push button "SavePlot" when      *
 *                 there was a click to this slot                               *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to select a file and save it       *
 ********************************************************************************/
void MainWindow::on_pushButton_savePlot_clicked()
{
    //if dir is empty Qmessage
    QDir dir(Path_selected + "/Resource_graphs");
    if (dir.isEmpty()){
        QMessageBox::warning(this, "", "No Plot are available to be saved");
        return;
    }
    QMessageBox msgBoxQuestion;
    msgBoxQuestion.setText(tr("To save plot from file(s) or folder of Resource_graphs folder to another place (first dialog selection)"));
    QAbstractButton* pButtonDir = msgBoxQuestion.addButton(tr("Directory"), QMessageBox::YesRole);
    QAbstractButton* pButtonFile = msgBoxQuestion.addButton(tr("File(s)"), QMessageBox::YesRole);
    QAbstractButton* pButtonCancel = msgBoxQuestion.addButton(tr("Cancel"), QMessageBox::NoRole);
    pButtonDir->setStyleSheet("color:black; font: bold large;\n");
    pButtonFile->setStyleSheet("color:black; font: bold large;\n");
    pButtonCancel->setStyleSheet("color:black; font: bold large;\n");
    msgBoxQuestion.setWindowTitle("Do you want to save a directory or one or more files");
    msgBoxQuestion.setStyleSheet("background-color:grey");

    msgBoxQuestion.exec();

    if(msgBoxQuestion.clickedButton() == pButtonCancel)
        return;

    QString dir_name = QFileDialog::getExistingDirectory(this, tr("Where do you want save your file(s)"), QDir::homePath(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir_name.isEmpty())
        return;
    if(msgBoxQuestion.clickedButton() == pButtonFile){
        QStringList file_save = QFileDialog::getOpenFileNames(this, "Select file(s) to be saved", Path_selected + "/Resource_graphs");
        if(file_save.isEmpty())
            return;
        for(int i=0; i<file_save.size(); i++){
            QFile file_saved(file_save[i]);
            if(file_saved.open(QFile::ReadOnly | QFile::Text)){
                QTextStream in(&file_saved);
                QString fileSaveName =  file_save[i].section("/", -1,-1);
                if(fileSaveName.contains(".txt")){
                    fileSaveName.replace(".txt","");
                }
                QString sDate = file_save[0].section("/",-2,-2);
                if(sDate.contains("graph")){
                    sDate.replace("graph","");
                }
                QFile file(dir_name + "/" + fileSaveName + sDate + ".txt");
                file_path = dir_name + "/" + fileSaveName + sDate + ".txt";
                if(!file.open(QFile::ReadWrite | QFile::Text)){
                       QMessageBox::warning(this, "WARNING", "The file wasn't saved");
                       return;
                }
                QTextStream out(&file);
                while(!in.atEnd()){
                   out << in.readLine();
                   out << "\n";
                }
                file.close();
            }
        }
    }
    if(msgBoxQuestion.clickedButton() == pButtonDir){
        QString fromDir = QFileDialog::getExistingDirectory(this, tr("Where do you want save your file(s)"), Path_selected + "/Resource_graphs", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if(fromDir.isEmpty())
            return;
        QString name_folder = fromDir.section("/", -1, -1);
        QString toDir = dir_name + "/" + name_folder;
        QDir directory(toDir);
        if(!directory.exists())
            dir.mkpath(toDir);
        bool copyAndRemove = false;
            QDirIterator it(fromDir, QDirIterator::Subdirectories);
            QDir dir(fromDir);
            const int absSourcePathLength = dir.absoluteFilePath(fromDir).length();

            while (it.hasNext()){
                it.next();
                const auto fileInfo = it.fileInfo();
                if(!fileInfo.isHidden()) { //filters dot and dotdot
                    const QString subPathStructure = fileInfo.absoluteFilePath().mid(absSourcePathLength);
                    const QString constructedAbsolutePath = toDir + subPathStructure;

                    if(fileInfo.isFile()) {
                        //Copy File to target directory

                        //Remove file at target location, if it exists, or QFile::copy will fail
                        QFile::remove(constructedAbsolutePath);
                        QFile::copy(fileInfo.absoluteFilePath(), constructedAbsolutePath);
                    }
                }
            }
            if(copyAndRemove)
                dir.removeRecursively();
        }
}


/********************************************************************************
 * Name : on_comboBox_currentIndexChanged                                       *
 *                                                                              *
 * Input variable: connect the signal from the "comboBox" then the index change *
 *                 to this slot                                                 *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to set SLEEP_TIME value            *
 ********************************************************************************/
void MainWindow::on_comboBox_currentIndexChanged() //&arg
{
    if(ui->comboBox->currentText() == "DEFAULT")
        SLEEP_TIME = 10000;
    else
        SLEEP_TIME = ui->comboBox->currentText().toInt();

}


/********************************************************************************
 * Name : on_pushButton_Plot_Stop_clicked                                        *
 *                                                                              *
 * Input variable: connect the signal from the push button "Stop" when there    *
 *                 was a click to this slot                                     *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to stop the running application    *
 ********************************************************************************/
void MainWindow::on_pushButton_Plot_Stop_clicked()
{
    //Stopped
    PlotThread->stop = true;
    usleep(500);
    StartAndStopThread->stop = true;
}


/********************************************************************************
 * Name : on_pushButton_Plot_Clear_clicked                                      *
 *                                                                              *
 * Input variable: connect the signal from the push button "Clear" when there   *
 *                 was a click to this slot                                     *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to clear "tableWidget_2" (all plot)*
 ********************************************************************************/
void MainWindow::on_pushButton_Plot_Clear_clicked()
{
    // Remove all items
    ui->tableWidget_2->clear();

    // Set row count to 0 (remove rows)
    ui->tableWidget_2->setRowCount(0);
}


/********************************************************************************
 * Name : runThread                                                             *
 *                                                                              *
 * Input variable: std::vector<double> instr_ret                                *
                   std::vector<double> counter_1                                *
                   std::vector<double> counter_2                                *
                   std::vector<double> counter_3                                *
                   std::vector<double> counter_4                                *
                   char *inCounter1                                             *
                   char *inCounter2                                             *
                   char *inCounter3                                             *
                   char *inCounter4                                             *
                   char *app                                                    *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to when "mythread" emit a signal   *
 *                      to do the last plots with the cache misses of counters  *
 *                      selected                                                *
 ********************************************************************************/
int MainWindow::runThread(std::vector<double> instr_ret,
                          std::vector<double> counter_1,
                          std::vector<double> counter_2,
                          std::vector<double> counter_3,
                          std::vector<double> counter_4,
                          char *inCounter1,
                          char *inCounter2,
                          char *inCounter3,
                          char *inCounter4,
                          char *app,
                          QString information)
{
    task_characteristic output;

    char* date_str = create_graph_folder(); //running time

    output.event_counter_sets.push_back(instr_ret);
    output.event_name.push_back((char *)malloc(sizeof(char) * strlen("PAPI_TOT_INS")));
    strcpy(output.event_name[0], "PAPI_TOT_INS");

    if(strcmp(inCounter1,"PAPI_NULL") == -2){
        output.event_counter_sets.push_back(counter_1);
        output.event_name.push_back((char*)malloc(sizeof(char)*strlen(inCounter1)));
        strcpy(output.event_name[1], inCounter1);
        plot_txt(&output, inCounter1, 1, app, date_str);
    }
    if(strcmp(inCounter2,"PAPI_NULL") == -2){
        output.event_counter_sets.push_back(counter_2);
        output.event_name.push_back((char*)malloc(sizeof(char)*strlen(inCounter2)));
        strcpy(output.event_name[2], inCounter2);
        plot_txt(&output, inCounter2, 2, app, date_str);
    }
    if(strcmp(inCounter3,"PAPI_NULL") == -2){
        output.event_counter_sets.push_back(counter_3);
        output.event_name.push_back((char*)malloc(sizeof(char)*strlen(inCounter3)));
        strcpy(output.event_name[3], inCounter3);
        plot_txt(&output, inCounter3, 3, app, date_str);
    }
    if(strcmp(inCounter4,"PAPI_NULL") == -2){
        output.event_counter_sets.push_back(counter_4);
        output.event_name.push_back((char*)malloc(sizeof(char)*strlen(inCounter4)));
        strcpy(output.event_name[4], inCounter4);
        plot_txt(&output, inCounter4, 4, app, date_str);
    }
    if (1 != WIFEXITED(status) || 0 != WEXITSTATUS(status))	{
        qDebug() << "Task 5 second characterization done!";
        return -1;
    }
    on_pushButton_Plot_Clear_clicked();

    for(int j=0; j<counters_vector.size(); j++){
        if(counters_vector[j]!= "PAPI_NULL"){
            QString file_name = Path_selected + "/Resource_graphs/" + date_str + "/" + app + "_" + counters_vector[j] + "_data.txt";
            readFilesPlots(file_name);
        }
    }
    QMessageBox MsgBox;
    MsgBox.about(this, "Information", information);
    return 0;
}


/********************************************************************************
 * Name : time_exec                                                             *
 *                                                                              *
 * Input variable: int time                                                     *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to show the execution time in      *
 *                      second                                                  *
 ********************************************************************************/
void MainWindow::time_exec(int time, int time_100)
{
    ui->label_time_exec->setText(QString::number(time));
    ui->progressBar->setValue(time_100);
    ui->progressBar->setStyleSheet("QProgressBar::chunk {background: QLinearGradient( x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #00BFFF, stop: 0.5 #729FCF, stop: 1 #4682B4);"
                                       "border-bottom-right-radius: 7px;"
                                       "border-bottom-left-radius: 2px;"
                                       "border: 1px solid black;}");
    if(time > (int)TASK_ACTIVE_MS *1000)
        PlotThread->stop = true;
}


/********************************************************************************
 * Name : startPlotThread                                                       *
 *                                                                              *
 * Input variable: std::vector<double> instr_ret                                *
                   std::vector<double> counter_1                                *
                   std::vector<double> counter_2                                *
                   std::vector<double> counter_3                                *
                   std::vector<double> counter_4                                *
                   char *inCounter1                                             *
                   char *inCounter2                                             *
                   char *inCounter3                                             *
                   char *inCounter4                                             *
                   char *app                                                    *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to when "mythread" emit a signal   *
 *                      to initialize variables of "mythreadPlot" and start it  *
 ********************************************************************************/
void MainWindow::startPlotThread(std::vector<double> instr_ret,
                                 std::vector<double> counter_1,
                                 std::vector<double> counter_2,
                                 std::vector<double> counter_3,
                                 std::vector<double> counter_4,
                                 char *inCounter1,
                                 char *inCounter2,
                                 char *inCounter3,
                                 char *inCounter4,
                                 char *app)
{ // vector<double> to double
    PlotThread->instr_ret_threadPlot = instr_ret;
    PlotThread->counter_1_threadPlot = counter_1;
    PlotThread->counter_2_threadPlot = counter_2;
    PlotThread->counter_3_threadPlot = counter_3;
    PlotThread->counter_4_threadPlot = counter_4;
    strcpy(PlotThread->inCounter1,inCounter1);
    strcpy(PlotThread->inCounter2,inCounter2);
    strcpy(PlotThread->inCounter3,inCounter3);
    strcpy(PlotThread->inCounter4,inCounter4);
    strcpy(PlotThread->app,app);

    PlotThread->start();
}


/********************************************************************************
 * Name : ThreadToPlot                                                          *
 *                                                                              *
 * Input variable: std::vector<double> instr_ret                                *
                   std::vector<double> counter_1                                *
                   std::vector<double> counter_2                                *
                   std::vector<double> counter_3                                *
                   std::vector<double> counter_4                                *
                   char *inCounter1                                             *
                   char *inCounter2                                             *
                   char *inCounter3                                             *
                   char *inCounter4                                             *
                   char *app                                                    *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to when "mythreadPlot" emit a      *
 *                      signal to do the plots with the cache misses of         *
 *                      counters selected                                       *
 ********************************************************************************/
void MainWindow::ThreadToPlot(std::vector<double> instr_ret_threadPlot,
                              std::vector<double> counter_1_threadPlot,
                              std::vector<double> counter_2_threadPlot,
                              std::vector<double> counter_3_threadPlot,
                              std::vector<double> counter_4_threadPlot,
                              char *inCounter1,
                              char *inCounter2,
                              char *inCounter3,
                              char *inCounter4,
                              char *app)
{
    task_characteristic output;
    QVector<int> iTime(500);
    QVector<float> cacheMisses(500);
    int number;

    output.event_counter_sets.push_back(instr_ret_threadPlot);
    output.event_name.push_back((char *)malloc(sizeof(char) * strlen("PAPI_TOT_INS")));
    strcpy(output.event_name[0], "PAPI_TOT_INS");

    if(strcmp(inCounter1,"PAPI_NULL") == -2){
        output.event_counter_sets.push_back(counter_1_threadPlot);
        output.event_name.push_back((char*)malloc(sizeof(char)*strlen(inCounter1)));
        strcpy(output.event_name[1], inCounter1);
    }
    if(strcmp(inCounter2,"PAPI_NULL") == -2){
        output.event_counter_sets.push_back(counter_2_threadPlot);
        output.event_name.push_back((char*)malloc(sizeof(char)*strlen(inCounter2)));
        strcpy(output.event_name[2], inCounter2);
    }
    if(strcmp(inCounter3,"PAPI_NULL") == -2){
        output.event_counter_sets.push_back(counter_3_threadPlot);
        output.event_name.push_back((char*)malloc(sizeof(char)*strlen(inCounter3)));
        strcpy(output.event_name[3], inCounter3);
    }
    if(strcmp(inCounter4,"PAPI_NULL") == -2){
        output.event_counter_sets.push_back(counter_4_threadPlot);
        output.event_name.push_back((char*)malloc(sizeof(char)*strlen(inCounter4)));
        strcpy(output.event_name[4], inCounter4);
    }
    on_pushButton_Plot_Clear_clicked();

    for(int j=0; j<counters_vector.size(); j++){
        iTime.clear();
        cacheMisses.clear();
        number = j + 1;
        if(counters_vector[j]!= "PAPI_NULL"){
            QString file_name = Path_selected + "/Resource_graphs/" + app + "_" + counters_vector[j] + "_data.txt";
            for (int i = 0; i <  output.event_counter_sets[0].size(); i ++){
                if(iTime.size()>499)
                    iTime.remove(0);
                iTime.append(i);
                if(cacheMisses.size()>499)
                    cacheMisses.remove(0);
                cacheMisses.append(output.event_counter_sets[number][i]);
            }
            createPlot(&iTime, &cacheMisses, file_name.section("/", -1,-1));
        }
    }
}


/********************************************************************************
 * Name : on_pushButton_Plot_Analysis_clicked                                   *
 *                                                                              *
 * Input variable: connect the signal from the push button "Analysis" when      *
 *                 there was a click to this slot                               *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to open DialogAnalysis class       *
 ********************************************************************************/
void MainWindow::on_pushButton_Plot_Analysis_clicked()
{
    if(STOP == 0){
        DialogAnalysis dialogAnalisys;
        dialogAnalisys.setModal(true);
        dialogAnalisys.exec();
    }
}



/**********************************************************************************************************************
 * MYTHREAD CPP                                                                                                       *
 **********************************************************************************************************************/

Mythread::Mythread(QObject *parent):
    QThread(parent)
{

}


/********************************************************************************
 * Name : run                                                                   *
 *                                                                              *
 * Input variable: -                                                            *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to run application selected and    *
 *                      emit signal to start "mythreadPlot"                     *
 ********************************************************************************/
void Mythread::run()
{    
    int i = 0;
    int skipped = 0;
    int totalValues = 0;
    counter_1.clear();
    counter_2.clear();
    counter_3.clear();
    counter_4.clear();
    instr_ret.clear();
    if (0 == (my_pid = fork())){
        i++;
        mon_pid = syscall(SYS_gettid);
        fflush(stdout);

        if (i == 1){
            //printf("Child pid is: %d\n", mon_pid);
        }
        if( -1 == execve(argument[0], argument, NULL)){
            exit(-1);
        }
    }
    if ((retval = PAPI_attach(ev_set, my_pid)) != PAPI_OK){
        printf("Monitor pid is: %d\n", my_pid);
        printf("Papi error: %s\n", strerror(errno));
        exit(-1);
    }
    if ((retval = PAPI_start(ev_set)) != PAPI_OK){
        printf("PAPI_start error %d: %s0\n", retval, PAPI_strerror(retval));
        exit(-1);
    }
    if (PAPI_reset(ev_set) != PAPI_OK){
        printf("PAPI_reset error %d: %s0\n", retval, PAPI_strerror(retval));
        exit(-1);
    }
    //clear graph
    while((0 == waitpid(my_pid, &status, WNOHANG))){
        QMutex mutex;
        mutex.lock();
        if ((retval = PAPI_read(ev_set, values)) != PAPI_OK){
          qDebug("Failed to read the events");
        }
        if(values[0] == 0){
            skipped ++;
        }
        PAPI_reset(ev_set);
        instr_ret.push_back(values[0]);
        if(strcmp(inCounter1,"PAPI_NULL") == -2)   counter_1.push_back(values[1]);
        if(strcmp(inCounter2,"PAPI_NULL") == -2)   counter_2.push_back(values[2]);
        if(strcmp(inCounter3,"PAPI_NULL") == -2)   counter_3.push_back(values[3]);
        if(strcmp(inCounter4,"PAPI_NULL") == -2)   counter_4.push_back(values[4]);

        totalValues++;

        auto timestamp_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> test = timestamp_end - timestamp_start;
        double time_100 = test.count()*100000/TASK_ACTIVE_MS;
        emit time_exec(test.count(), time_100);
        qRegisterMetaType<std::vector<double>>();

        //just send the lqst value (counter_1.last)
        emit startPlot(instr_ret, counter_1, counter_2, counter_3, counter_4, inCounter1, inCounter2, inCounter3, inCounter4, app);


        if (((test.count() * 1000 > TASK_ACTIVE_MS)) || this->stop){
            kill(my_pid, SIGKILL);
            PAPI_reset(ev_set);
            PAPI_stop(ev_set, values);            
            if(this->stop)
                qDebug() << "Kill Process with STOP";
            else
                qDebug() << "Kill Process, time limit reached";
            msleep(100);

            QString information = "Running application: " + QString::fromLocal8Bit(argument[0]).section("/", -1,-1);
                    information += "\nExectution time (s): " + QString::number(test.count());
                    information += "\nTotal values:" + QString::number(totalValues);
                    information += "\nThere are " + QString::number(skipped) + " value(s)" + " out of " + QString::number(totalValues)+ " equal 0";
            emit runStartAndStop(instr_ret, counter_1, counter_2, counter_3, counter_4, inCounter1, inCounter2, inCounter3, inCounter4, app, information);
            STOP = 0;
            break;
        }
        mutex.unlock();
        usleep(SLEEP_TIME); //sampling
    }
}


/**********************************************************************************************************************
 * MYTHREADPLOT CPP                                                                                                       *
 **********************************************************************************************************************/

MyThreadPlot::MyThreadPlot(QObject *parent):
    QThread(parent)
{

}


/********************************************************************************
 * Name : run                                                                   *
 *                                                                              *
 * Input variable: -                                                            *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to plot the cache misses           *
 ********************************************************************************/
void MyThreadPlot::run()
{
    QMutex mutex;
    mutex.lock();
    emit readCounterToPlot(instr_ret_threadPlot, counter_1_threadPlot, counter_2_threadPlot, counter_3_threadPlot, counter_4_threadPlot, inCounter1, inCounter2, inCounter3, inCounter4, app);
    mutex.unlock();
    this->usleep(SLEEP_TIME);
}


/********************************************************************************
 * Name : on_comboBox_timeMax_currentIndexChanged                               *
 *                                                                              *
 * Input variable: connect the signal from the "comboBox_timeMax" then the      *
 *                 index change to this                                         *
 *                                                                              *
 * Output variable: -                                                           *
 *                                                                              *
 * Summary of function: This function allows to set SLEEP_TIME value            *
 ********************************************************************************/
void MainWindow::on_comboBox_timeMax_currentIndexChanged(const QString &arg)
{
    if(arg == "DEFAULT")
        TASK_ACTIVE_MS = 10000;
    else
        TASK_ACTIVE_MS = arg.toInt()*1000;
}


