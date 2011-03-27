#include "hddinterface.h"
#include "widget.h"


//Pour allouer un objet de type hddInterface
HddInterface *HddInterface::createHddInterface(ConfigurationData *configurationData, NetworkInterface *networkInterface,QStandardItemModel *model)
{
	if(configurationData==NULL) return NULL;
	if(networkInterface==NULL) return NULL;
	if(model==NULL) return NULL;
	return new HddInterface(configurationData,networkInterface,model);
}


//Le constructeur
HddInterface::HddInterface(ConfigurationData *configurationData, NetworkInterface *networkInterface,QStandardItemModel *model):QObject()
{
	this->networkInterface=networkInterface;
	this->model=model;
	this->configurationData=configurationData;
	this->configurationData->getConfigurationFile()->setSignalListener(this);
	QObject::connect(networkInterface,SIGNAL(receiveCreatedMediaMessage(Dir*,QString)),this,SLOT(receiveCreatedMediaMessageAction(Dir*,QString)));
	QObject::connect(networkInterface,SIGNAL(receiveModifiedFileMessage(File*,QByteArray)),this,SLOT(receiveModifiedFileMessageAction(File*,QByteArray)));
	QObject::connect(networkInterface,SIGNAL(receiveRemovedMediaMessage(Media*)),this,SLOT(receiveRemovedMediaMessageAction(Media*)));
	QObject::connect(networkInterface,SIGNAL(receiveErrorMessage(QString)),this,SLOT(receiveErrorMessageAction(QString)));
}


//Lorsqu'un média est créé...
void HddInterface::mediaHasBeenCreated(Media *m, Dir *parent)
{
	networkInterface->sendMediaCreated(m);
	parent->getSubMedias()->append(m);
	if(m->isDirectory())
	{
		Dir *d=(Dir*)m;
		d->setSignalListener(this);
	}
	Widget::addRowToTable("Le media "+m->getLocalPath()+" a été créé",model);
	configurationData->save();
}


//Lorsqu'un média est supprimé
void HddInterface::mediaHasBeenRemoved(Media *m, Dir *parent)
{
	networkInterface->sendMediaRemoved(m);
	for(int i=0;i<parent->getSubMedias()->size();i++)
	{
		if(m->getLocalPath()==parent->getSubMedias()->at(i)->getLocalPath())
		{
			parent->getSubMedias()->remove(i);
			break;
		}
	}
	Widget::addRowToTable("Le media "+m->getLocalPath()+" a été supprimé",model);
	if(m->isDirectory())
	{
		Dir *d=(Dir*)m;
		d->setSignalListener(NULL);
	}
	delete m;
	configurationData->save();
}


//Lorsqu'un fichier est modifié
void HddInterface::fileHasBeenUpdated(File *f, Dir *parent)
{
	QFile file(f->getLocalPath());
	file.open(QIODevice::ReadOnly);
	networkInterface->sendFileModified(f,file.readAll());
	file.close();
	f->updateContent();
	Widget::addRowToTable("Le fichier "+f->getLocalPath()+" a été modifié",model);
	configurationData->save();
	parent=parent;
}


//on a recu la modif d'un fichier
void HddInterface::receiveModifiedFileMessageAction(File *f,QByteArray content)
{
	Widget::addRowToTable("Message du serveur: le fichier "+f->getLocalPath()+" a été modifié",model);
	Dir *dir=configurationData->getConfigurationFile()->findMediaParentByLocalPath(f->getLocalPath());
	if(!dir) return;
	dir->setSignalListener(NULL);
	QFile file(f->getLocalPath());
	if(!file.open(QIODevice::WriteOnly)) return;
	file.write(content);
	file.close();
	f->updateContent();
	dir->setSignalListener(this);
	configurationData->save();
}



void HddInterface::receiveCreatedMediaMessageAction(Dir *parent,QString realName)
{
	parent->setSignalListener(NULL);
	if(realName.endsWith("/"))
	{
		realName=realName.left(realName.length()-1);
		QDir dir(parent->getLocalPath());
		if(!dir.mkdir(realName)) return;
		Dir *d=Dir::createDir(parent->getLocalPath()+"/"+realName,parent->getRealPath()+"/"+realName);
		if(!d) return ;
		Widget::addRowToTable("Message du serveur: le repertoire "+d->getLocalPath()+" a été créé",model);
		parent->getSubMedias()->append(d);
		d->setSignalListener(this);
	}
	else
	{
		QFile file(parent->getLocalPath()+"/"+realName);
		if(!file.open(QIODevice::WriteOnly)) return;
		file.close();
		File *f=File::createFile(parent->getLocalPath()+"/"+realName,parent->getRealPath()+"/"+realName);
		if(!f) return;
		Widget::addRowToTable("Message du serveur: le fichier "+f->getLocalPath()+" a été créé",model);
		parent->getSubMedias()->append(f);
	}
	parent->setSignalListener(this);
	configurationData->save();
}


void rmNonEmptyDir( QString name )
{
    QDir dir( name );
    QFileInfoList fileList = dir.entryInfoList( QDir::Files | QDir::Hidden );
    foreach(QFileInfo file, fileList) dir.remove( file.absoluteFilePath());
    QFileInfoList dirList = dir.entryInfoList( QDir::AllDirs | QDir::Hidden | QDir::NoDotAndDotDot );
    foreach(QFileInfo dir, dirList) rmNonEmptyDir(dir.absoluteFilePath());
    dir.rmdir(dir.absolutePath());
}


void HddInterface::receiveRemovedMediaMessageAction(Media *m)
{
	Dir *parent=configurationData->getConfigurationFile()->findMediaParentByLocalPath(m->getLocalPath());
	if(!parent) return;
	parent->setSignalListener(NULL);
	for(int i=0;i<parent->getSubMedias()->size();i++)
	{
		if(m==parent->getSubMedias()->at(i))
		{
			parent->getSubMedias()->remove(i);
			break;
		}
	}
	if(m->isDirectory())
	{
		Dir *d=(Dir*)m;
		Widget::addRowToTable("Message du serveur: le repertoire "+d->getLocalPath()+" a été supprimé",model);
		d->setSignalListener(NULL);
		rmNonEmptyDir(d->getLocalPath());
		delete d;
	}
	else
	{
		File *f=(File*)m;
		Widget::addRowToTable("Message du serveur: le fichier "+f->getLocalPath()+" a été supprimé",model);
		QFile::remove(f->getLocalPath());
		delete f;
	}
	parent->setSignalListener(this);
	configurationData->save();
}





void HddInterface::receiveErrorMessageAction(QString s)
{
	QMessageBox::information(NULL,"",s);
}



