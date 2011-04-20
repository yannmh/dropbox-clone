#include "widget.h"


//Le constructeur
Widget::Widget(): QWidget()
{
	//On initialise la fenetre et ses composants
	this->setWindowTitle("Serveur");
	setMinimumSize(640,480);
	QTableView *tableView=new QTableView();
	tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	this->model=new QStandardItemModel(0,2,tableView);
	QStringList list;
	list<<"Evenement"<<"Heure";
	model->setHorizontalHeaderLabels(list);
	tableView->setModel(model);
	tableView->horizontalHeader()->setStretchLastSection(true);
	tableView->setColumnWidth(0,500);
	QHBoxLayout *layout = new QHBoxLayout;
	layout->addWidget(tableView);
	this->setLayout(layout);
	addRowToTable("Démarage de l'application",model);

	//On créé la bdd
	DatabaseManager *db=DatabaseManager::createDatabaseManager("dropbox","user","mdp");
	if(db) addRowToTable("La base de données a été créé",model,false);
	else {addRowToTable("Erreur à la création de la base de données",model,false);return;}

	//On créé le fileManager
	SvnManager *svn=SvnManager::createSvnManager("hky","hky","svn","svnadmin");
	FileManager *f=FileManager::createFileManager(svn);
	if(f) addRowToTable("Le gestionnaire de fichiers a été créé",model,false);
	else {addRowToTable("Erreur à la création du gestionnaire de fichiers",model,false);return;}
	QList<SqlDepot*> depots=db->getDepots();
	for(int i=0;i<depots.length();i++) f->addDepot(depots.at(i)->depotname);

	//On créé l'interface réseau
	this->server=Server::createServer(db,f,model);
	bool result=this->server->beginListenning(4321);
	if(result) addRowToTable("Le serveur est démarré sur le port 4321",model,false);
	else {addRowToTable("Echec de démarrage du serveur sur le port 4321",model,false);return;}
}


//Juste une méthode statique qui écrit un évenement dans le model, et l'heure à laquelle il s'est réalisé
void Widget::addRowToTable(QString s,QStandardItemModel *model,bool changeColor)
{
	static int r=255;
	static int g=125;
	static int b=0;
	if(changeColor)
	{
		r=(r==255)?0:((r==125)?255:125);
		b=(b==255)?0:((b==125)?255:125);
		g=(g==255)?0:((g==125)?255:125);
	}
	QList<QStandardItem*> list;
	QStandardItem *i1=new QStandardItem(trUtf8(s.toAscii()));
	i1->setBackground(QBrush(QColor::fromRgb(r,g,b)));
	QStandardItem *i2=new QStandardItem(QTime::currentTime().toString("hh:mm:ss"));
	i2->setBackground(QBrush(QColor::fromRgb(r,g,b)));
	list << i1 << i2;
	model->appendRow(list);
}
