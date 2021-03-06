#include "media.h"




//le constructeur pour initialiser les attributs
Media::Media(QString localPath,QString realPath,Dir *parent): QObject((QObject*)parent)
{
	this->localPath=localPath;
	this->realPath=realPath;
	this->parent=parent;
	this->mutex=new QMutex(QMutex::Recursive); //Le mutex peut être verouiller récursivement
	this->detectionState=new QList<State>();
}



//les accesseurs d'accès et de modifications au localPath

QString Media::getLocalPath()
{
	return localPath;
}

void Media::setLocalPath(QString localPath)
{
	this->localPath=localPath;
}




//les accesseurs d'accès et de modifications au realPath

QString Media::getRealPath()
{
	return realPath;
}

void Media::setRealPath(QString realPath)
{
	this->realPath=realPath;
}




//les accesseurs d'accès et de modifications au parent

Dir *Media::getParent()
{
	return parent;
}

void Media::setParent(Dir *parent)
{
	this->parent=parent;
}



//l'accesseur d'accès à la liste des détections en cours de traitement
QList<State> *Media::getDetectionState()
{
	return detectionState;
}




//Pour réserver l'objet
void Media::lock()
{
    this->mutex->lock();
}




//Pour le libérer
void Media::unlock()
{
    this->mutex->unlock();
}




//méthode statique qui extrait d'un chemin, son repertoire parent.
//le resultat est renvoyé sans le "/"
QString Media::extractParentPath(QString path)
{
	if(path.endsWith("/")) path=path.left(path.length()-1);
	QString parentPath;
	int last = path.lastIndexOf("/");
	if(last>=0)
		parentPath=path.left(last);
	return parentPath;
}






//méthode statique qui extrait d'un chemin, son nom
//si c'est un repertoire, le nom retourné contient bien un "/" final
QString Media::extractName(QString path)
{
	QString path2=path;
	if(path2.endsWith("/")) path2=path2.left(path2.length()-1);
	QString name=path2;
	if(path2.lastIndexOf("/")>=0)
		name=path2.right(path2.length()-path2.lastIndexOf("/")-1);
	if(path.endsWith("/")) name=name+"/";
	return name;
}






//Méthode qui convertit une QString en une vrai state
State Media::stateFromString(QString stateString)
{
	State state;
	if(stateString=="MediaIsCreating") state=MediaIsCreating;
	else if(stateString=="MediaIsUpdating") state=MediaIsUpdating;
	else if(stateString=="MediaIsRemoving") state=MediaIsRemoving;
	else state=MediaDefaultState;
	return state;
}







//Méthode qui convertit une state en une QString
QString Media::stateToString(State state)
{
	QString stateString;
	if(state==MediaIsCreating) stateString="MediaIsCreating";
	else if(state==MediaIsUpdating) stateString="MediaIsUpdating";
	else if(state==MediaIsRemoving) stateString="MediaIsRemoving";
	else stateString="MediaDefaultState";
	return stateString;
}






//destructeur
Media::~Media()
{
	this->lock();
	this->unlock();
	detectionState->clear();
	delete detectionState;
}
